/**
 * @file light_ubo_uploader.cpp
 * @brief LightUboUploader 구현 - std140 LightBlock 패킹(UBO 경로) + loose glUniform 송신(공존 경로).
 *
 * @details
 *  ### 구현 흐름
 *  - @c Update : 광원 -> std140 @c LightBlockStd140 패킹 -> 공유 UBO 업로드 + loose 경로용 원시 캐시.
 *  - @c BindTo : program 순회 - LightBlock 보유 program 은 소유권 회수 후 공유 UBO 결속,
 *    lighting sentinel(@c UNI_VIEW_POS) 보유 loose program 은 @c glUniform* 송신, 그 외 skip.
 *
 *  ### std140 미러 (phong.refl.json 실측 2026-06-21)
 *  파일-로컬 @c LightBlockStd140 가 phong.slang 의 LightBlock 과 byte 단위 1:1. 모든 offset/size 는
 *  @c static_assert 로 컴파일타임 검증 - 셰이더 layout 변경 시 빌드가 즉시 차단된다.
 *
 *  ### 비-책임
 *  - [X] Light 컴포넌트 수집 -> @c SceneRenderer::RenderWithCamera.
 *  - [X] 감쇠 계수 계산 -> @c GetAttenuationCoeff (@c object/light.h 자유 함수).
 */
#include "render/light_ubo_uploader.h"
#include "common/constants.h"
#include "object/light.h" // GetAttenuationCoeff (거리 감쇠 계수 자유 함수)
#include "scene/light.h"  // DirLight/PointLight/SpotLight 컴포넌트 (2026-06-11 E1 이주처)
#include "program/program.h"
#include "program/program_uniforms.h"
#include "program/uniform_buffer.h" // 공유 LightBlock UBO complete type (헤더는 forward-decl 만 - GL-free)
#include "render/device_context.h"
#include <algorithm>        // std::min
#include <cmath>            // cosf - SpotLight degree->cosine 변환 (D6 이주)
#include <cstddef>          // offsetof
#include <cstdint>          // std::int32_t
#include <cstdlib>          // std::abort - UBO 생성 실패 fail-fast
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace SJH
{
	namespace
	{
		// ====================================================================
		//  std140 미러 - apps/_MyApp_/shaders_slang/phong.slang 의 LightBlock 과 byte 단위 1:1.
		//  모든 offset/size 는 phong.refl.json 실측(2026-06-21) + static_assert 로 검증.
		//  vec3 는 std140 에서 16B 정렬 -> 뒤에 float pad 1개.
		//  SpotLight 의 cutoff 는 direction vec3 의 4번째 슬롯(@28)에 패킹됨 (refl.json 비대칭).
		// ====================================================================
		struct DirLightStd140
		{
			vmath::vec3 direction; float pad0;
			vmath::vec3 ambient;   float pad1;
			vmath::vec3 diffuse;   float pad2;
			vmath::vec3 specular;  float pad3;
		};
		struct PointLightStd140
		{
			vmath::vec3 position;    float pad0;
			vmath::vec3 attenuation; float pad1;
			vmath::vec3 ambient;     float pad2;
			vmath::vec3 diffuse;     float pad3;
			vmath::vec3 specular;    float pad4;
		};
		struct SpotLightStd140
		{
			vmath::vec3 position;    float pad0;       // @0   position(12) + pad
			vmath::vec3 direction;   float cutoff;     // @16  direction(12) + cutoff @28 (4번째 슬롯)
			float       outerCutoff; float pad1[3];    // @32  outerCutoff + pad -> @48
			vmath::vec3 attenuation; float pad2;        // @48
			vmath::vec3 ambient;     float pad3;        // @64
			vmath::vec3 diffuse;     float pad4;        // @80
			vmath::vec3 specular;    float pad5;        // @96
		};
		struct LightBlockStd140
		{
			DirLightStd140   dirLight;                              // @0
			PointLightStd140 pointLights[Const::MAX_POINT_LIGHTS];  // @64
			SpotLightStd140  spotLights[Const::MAX_SPOT_LIGHTS];    // @1344
			vmath::vec3      viewPos;                               // @3136
			std::int32_t     dirLightEnabled;                      // @3148
			std::int32_t     numPointLights;                       // @3152
			std::int32_t     numSpotLights;                        // @3156
			std::int32_t     tailPad[2];                           // @3160 -> std140 16배수 (size 3168)
		};

		// 컴파일타임 std140 정합 검증 (phong.refl.json offset 과 1:1) - 어긋나면 빌드 차단.
		static_assert(sizeof(vmath::vec3) == 12, "vmath::vec3 must be 12B for std140 mirror");
		static_assert(sizeof(DirLightStd140) == 64, "DirLightStd140 std140 size");
		static_assert(sizeof(PointLightStd140) == 80, "PointLightStd140 std140 stride");
		static_assert(sizeof(SpotLightStd140) == 112, "SpotLightStd140 std140 stride");
		static_assert(offsetof(SpotLightStd140, cutoff) == 28, "SpotLight cutoff @28");
		static_assert(offsetof(SpotLightStd140, outerCutoff) == 32, "SpotLight outerCutoff @32");
		static_assert(offsetof(SpotLightStd140, attenuation) == 48, "SpotLight attenuation @48");
		static_assert(offsetof(LightBlockStd140, pointLights) == 64, "LightBlock pointLights @64");
		static_assert(offsetof(LightBlockStd140, spotLights) == 1344, "LightBlock spotLights @1344");
		static_assert(offsetof(LightBlockStd140, viewPos) == 3136, "LightBlock viewPos @3136");
		static_assert(offsetof(LightBlockStd140, dirLightEnabled) == 3148, "LightBlock dirLightEnabled @3148");
		static_assert(offsetof(LightBlockStd140, numPointLights) == 3152, "LightBlock numPointLights @3152");
		static_assert(offsetof(LightBlockStd140, numSpotLights) == 3156, "LightBlock numSpotLights @3156");
		static_assert(sizeof(LightBlockStd140) == 3168, "LightBlock std140 total (16-rounded)");

		// 셰이더 측 LightBlock 정규화 이름 (Program::NormalizeBlockName: "block_LightBlock_0" -> "LightBlock").
		const char* const LIGHT_BLOCK_NAME = "LightBlock";

		// ====================================================================
		//  loose 경로 헬퍼 (구 LightUniformDispatcher 에서 보존 - D6 격리, S4 공존).
		//  program -> object 역의존을 끊기 위해 광원 struct -> uniform 변환을 익명 ns 에 가둔다.
		// ====================================================================
		void SetDirLight(const Program& prog, const char* prefix, const DirLight& light, const vmath::vec3& worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(), worldDir);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(), light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(), light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(), light.Specular);
		}

		void SetPointLight(const Program& prog, const char* prefix, const PointLight& light, const vmath::vec3& worldPos)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(), worldPos);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(), GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(), light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(), light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(), light.Specular);
		}

		void SetSpotLight(const Program& prog, const char* prefix, const SpotLight& light,
		                  const vmath::vec3& worldPos, const vmath::vec3& worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(), worldPos);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(), worldDir);
			// CPU 는 degree, 셰이더는 cosine - 송신 시점에 변환 (struct 정의 시 의도된 분업).
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_CUTOFF).c_str(), cosf(vmath::radians(light.CutoffAngleDeg)));
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_OUTER_CUTOFF).c_str(), cosf(vmath::radians(light.OuterCutoffAngleDeg)));
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(), GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(), light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(), light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(), light.Specular);
		}

		// loose 송신 - 비-UBO lighting program 1개에 광원 일괄 송신 (구 Dispatch 루프 본체).
		//   MAX 슬롯 전체 순회 - 활성 광원은 값+enabled=1, 슬롯 부족분은 enabled=0 (셰이더 초기화 보장).
		void LooseDispatch(const Program& prog, DirLight* dir,
		                   const std::vector<PointLight*>& points,
		                   const std::vector<SpotLight*>& spots,
		                   const vmath::vec3& viewPos)
		{
			auto& rc = DeviceContext::Get();
			rc.UseProgram(prog);

			Uniforms::SetVec3(prog, Const::UNI_VIEW_POS, viewPos);

			if (dir)
			{
				SetDirLight(prog, Const::UNI_DIR_LIGHT, *dir, dir->GetWorldDirection());
				Uniforms::SetInt(prog, Const::UNI_DIR_LIGHT_ENABLED, 1);
			}
			else
			{
				Uniforms::SetInt(prog, Const::UNI_DIR_LIGHT_ENABLED, 0);
			}

			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_POINT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_POINT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_POINT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < points.size())
				{
					SetPointLight(prog, idxStr.c_str(), *points[i], points[i]->GetWorldPosition());
					Uniforms::SetInt(prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(prog, enStr.c_str(), 0);
				}
			}

			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_SPOT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_SPOT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < spots.size())
				{
					SetSpotLight(prog, idxStr.c_str(), *spots[i], spots[i]->GetWorldPosition(), spots[i]->GetWorldDirection());
					Uniforms::SetInt(prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(prog, enStr.c_str(), 0);
				}
			}
		}
	} // anonymous namespace

	// ctor/dtor out-of-line - mLightBlockUbo(unique_ptr<UniformBuffer>) 가 여기서 complete type.
	LightUboUploader::LightUboUploader()  = default;
	LightUboUploader::~LightUboUploader() = default;

	void LightUboUploader::Update(DirLight* dir,
	                              const std::vector<PointLight*>& points,
	                              const std::vector<SpotLight*>& spots,
	                              const vmath::vec3& viewPos)
	{
		if (static_cast<int>(points.size()) > Const::MAX_POINT_LIGHTS)
			spdlog::warn("LightUboUploader - PointLight {} 개 발견. MAX_POINT_LIGHTS={} 초과분 무시.",
			             points.size(), Const::MAX_POINT_LIGHTS);
		if (static_cast<int>(spots.size()) > Const::MAX_SPOT_LIGHTS)
			spdlog::warn("LightUboUploader - SpotLight {} 개 발견. MAX_SPOT_LIGHTS={} 초과분 무시.",
			             spots.size(), Const::MAX_SPOT_LIGHTS);

		// 1) loose 경로용 원시 캐시 (BindTo 가 비-UBO lighting program 에 재송신) - 프레임 내 유효, 비소유.
		mDirPtr  = dir;
		mPoints  = points;
		mSpots   = spots;
		mViewPos = viewPos;

		// 2) std140 LightBlock 패킹.
		LightBlockStd140 block{};
		if (dir)
		{
			block.dirLight.direction = dir->GetWorldDirection();
			block.dirLight.ambient   = dir->Ambient;
			block.dirLight.diffuse   = dir->Diffuse;
			block.dirLight.specular  = dir->Specular;
			block.dirLightEnabled    = 1;
		}

		const std::size_t np = std::min(points.size(), static_cast<std::size_t>(Const::MAX_POINT_LIGHTS));
		for (std::size_t i = 0; i < np; ++i)
		{
			PointLightStd140& dst = block.pointLights[i];
			const PointLight& src = *points[i];
			dst.position    = src.GetWorldPosition();
			dst.attenuation = GetAttenuationCoeff(src.Distance);
			dst.ambient     = src.Ambient;
			dst.diffuse     = src.Diffuse;
			dst.specular    = src.Specular;
		}
		block.numPointLights = static_cast<std::int32_t>(np);

		const std::size_t ns = std::min(spots.size(), static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS));
		for (std::size_t i = 0; i < ns; ++i)
		{
			SpotLightStd140& dst = block.spotLights[i];
			const SpotLight& src = *spots[i];
			dst.position    = src.GetWorldPosition();
			dst.direction   = src.GetWorldDirection();
			dst.cutoff      = cosf(vmath::radians(src.CutoffAngleDeg));
			dst.outerCutoff = cosf(vmath::radians(src.OuterCutoffAngleDeg));
			dst.attenuation = GetAttenuationCoeff(src.Distance);
			dst.ambient     = src.Ambient;
			dst.diffuse     = src.Diffuse;
			dst.specular    = src.Specular;
		}
		block.numSpotLights = static_cast<std::int32_t>(ns);

		block.viewPos = viewPos;

		// 3) 공유 UBO lazy create + 전체 업로드 (fail-fast - Phase 2 UBO 정책 일관).
		if (!mLightBlockUbo)
		{
			mLightBlockUbo = UniformBuffer::Create(sizeof(LightBlockStd140));
			if (!mLightBlockUbo)
			{
				spdlog::critical("LightUboUploader - LightBlock UBO 생성 실패 (size={}).", sizeof(LightBlockStd140));
				std::abort();
			}
		}
		mLightBlockUbo->Update(&block, sizeof(block), 0);
	}

	void LightUboUploader::BindTo(const std::vector<Program*>& programs)
	{
		for (Program* prog : programs)
		{
			if (!prog)
				continue;

			// UBO 경로 - LightBlock 보유 program: 소유권 회수 후 공유 UBO 를 그 binding point 에 결속.
			//   DisownUniformBlock 으로 per-program UBO 를 해제하면 mesh_pass 의 BindUniformBlocks 가
			//   해당 블록을 skip -> 여기서 결속한 공유 UBO 가 draw 까지 유지된다 (binding point 는 global state).
			if (const Program::UniformBlock* blk = prog->FindUniformBlock(LIGHT_BLOCK_NAME))
			{
				const GLuint bindingPoint = blk->bindingPoint;
				prog->DisownUniformBlock(LIGHT_BLOCK_NAME);
				if (mLightBlockUbo)
					mLightBlockUbo->BindBase(bindingPoint);
				continue;
			}

			// loose 경로 - lighting sentinel(UNI_VIEW_POS) 보유 program: glUniform* 송신 (S4 공존).
			//   UNI_VIEW_POS 가 없으면 lighting 미사용(simple/passthrough/postfx) -> skip.
			if (prog->GetLocation(Const::UNI_VIEW_POS) >= 0)
				LooseDispatch(*prog, mDirPtr, mPoints, mSpots, mViewPos);
		}
	}

} // namespace SJH
