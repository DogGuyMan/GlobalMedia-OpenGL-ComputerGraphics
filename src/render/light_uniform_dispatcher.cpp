/**
 * @file light_uniform_dispatcher.cpp
 * @brief LightUniformDispatcher::Dispatch 구현 - Program 순회 + lighting sentinel + 일괄 uniform 송신.
 *
 * @details
 *  ### 구현 흐름 (Dispatch)
 *  1. 초과 경고 - PointLight/SpotLight 개수가 @c MAX_POINT_LIGHTS / @c MAX_SPOT_LIGHTS 초과 시
 *     @c spdlog::warn (초과분은 이후 for 루프에서 자동 무시).
 *  2. @c ResourceRegistry::GetAllPrograms() 스냅샷 순회:
 *     - @c prog->GetLocation(UNI_VIEW_POS) < 0 이면 skip (lighting sentinel - simple/PostFX 셰이더 차단).
 *     - @c DeviceContext::UseProgram(*prog) 활성화.
 *     - viewPos -> DirLight(1개) -> PointLight 배열 -> SpotLight 배열 순으로 uniform 송신.
 *  3. PointLight / SpotLight 배열: @c MAX_*_LIGHTS 슬롯 전체 순회 - 활성 광원은 값 + enabled=1,
 *     슬롯 부족분은 enabled=0 만 전송하여 셰이더 쪽 초기화 보장.
 *
 *  ### 비-책임
 *  - [X] Light 컴포넌트 수집 -> @c SceneRenderer::RenderWithCamera 담당.
 *  - [X] 감쇠 계수(Kc/Kl/Kq) 계산 -> @c GetAttenuationCoeff 자유 함수(@c object/light.h) 담당.
 */
#include "render/light_uniform_dispatcher.h"
#include "common/constants.h"
#include "object/light.h"   // GetAttenuationCoeff (거리 감쇠 계수 자유 함수)
#include "scene/light.h"    // DirLight/PointLight/SpotLight 컴포넌트 (2026-06-11 E1 이주처)
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/device_context.h"
#include <cmath>            // cosf - SpotLight degree->cosine 변환 (D6 이주)
#include <spdlog/spdlog.h>
#include <string>
#include <vector>

namespace SJH
{
	namespace
	{
		// D6 (2026-06-11): program_uniforms.cpp 에서 이주한 광원 struct -> uniform block 일괄 전송 헬퍼.
		// program -> object 역의존(program 이 광원 타입을 참조)을 끊기 위해 유일 호출처인 본 dispatcher 의
		// 파일-로컬(익명 ns)로 옮겼다. 실제 GL 송신은 Uniforms::SetVec3/SetFloat 재사용 - 캐시/진단 경로 동일.
		void SetDirLight(const Program &prog, const char *prefix,
		                 const DirLight &light, const vmath::vec3 &worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(), worldDir);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),   light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),   light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),  light.Specular);
		}

		void SetPointLight(const Program &prog, const char *prefix,
		                   const PointLight &light, const vmath::vec3 &worldPos)
		{
			const std::string base = prefix;
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),    worldPos);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(), GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),     light.Ambient);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),     light.Diffuse);
			Uniforms::SetVec3(prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),    light.Specular);
		}

		void SetSpotLight(const Program &prog, const char *prefix,
		                  const SpotLight &light, const vmath::vec3 &worldPos, const vmath::vec3 &worldDir)
		{
			const std::string base = prefix;
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_POSITION).c_str(),     worldPos);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIRECTION).c_str(),    worldDir);
			// CPU 는 degree, 셰이더는 cosine - 송신 시점에 변환 (struct 정의 시 의도된 분업).
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_CUTOFF).c_str(),       cosf(vmath::radians(light.CutoffAngleDeg)));
			Uniforms::SetFloat(prog, (base + Const::SHADER_PROPERTIE_OUTER_CUTOFF).c_str(), cosf(vmath::radians(light.OuterCutoffAngleDeg)));
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_ATTENUATION).c_str(),  GetAttenuationCoeff(light.Distance));
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_AMBIENT).c_str(),      light.Ambient);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_DIFFUSE).c_str(),      light.Diffuse);
			Uniforms::SetVec3 (prog, (base + Const::SHADER_PROPERTIE_SPECULAR).c_str(),     light.Specular);
		}
	} // anonymous namespace

	void LightUniformDispatcher::Dispatch(const std::vector<Program *> &programs,
	                                      DirLight *dir,
	                                      const std::vector<PointLight *> &points,
	                                      const std::vector<SpotLight *> &spots,
	                                      const vmath::vec3 &viewPos)
	{
		if (static_cast<int>(points.size()) > Const::MAX_POINT_LIGHTS)
			spdlog::warn("LightUniformDispatcher - PointLight {} 개 발견. 셰이더 MAX_POINT_LIGHTS={} 초과분 무시.",
			             points.size(), Const::MAX_POINT_LIGHTS);
		if (static_cast<int>(spots.size()) > Const::MAX_SPOT_LIGHTS)
			spdlog::warn("LightUniformDispatcher - SpotLight {} 개 발견. 셰이더 MAX_SPOT_LIGHTS={} 초과분 무시.",
			             spots.size(), Const::MAX_SPOT_LIGHTS);

		auto &rc = DeviceContext::Get();
		for (const auto *prog : programs)
		{
			if (!prog)
				continue;
			// Lighting schema sentinel - UNI_VIEW_POS 가 UniformCache 에 없으면
			// 이 program 은 lighting 미사용 (simple/passthrough/postfx 등) ->송신 통째 skip.
			// warn-once 노이즈 차단 + glUseProgram 비용 회피.
			if (prog->GetLocation(Const::UNI_VIEW_POS) < 0)
				continue;
			rc.UseProgram(*prog);

			// viewPos - Phong specular 계산용.
			Uniforms::SetVec3(*prog, Const::UNI_VIEW_POS, viewPos);

			// DirLight - 1개. 없으면 enabled=0 만 전송 (uniform 0 보장).
			if (dir)
			{
				SetDirLight(*prog, Const::UNI_DIR_LIGHT, *dir, dir->GetWorldDirection());
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 1);
			}
			else
			{
				Uniforms::SetInt(*prog, Const::UNI_DIR_LIGHT_ENABLED, 0);
			}

			// PointLights - 최대 MAX_POINT_LIGHTS 개. 초과는 무시. 부족하면 enabled=0 으로 slot 채움.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_POINT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_POINT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_POINT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < points.size())
				{
					SetPointLight(*prog, idxStr.c_str(), *points[i], points[i]->GetWorldPosition());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}

			// SpotLights - 최대 MAX_SPOT_LIGHTS 개. PointLights 와 완전 동일 패턴.
			for (std::size_t i = 0; i < static_cast<std::size_t>(Const::MAX_SPOT_LIGHTS); ++i)
			{
				const std::string idxStr = Const::UNI_SPOT_LIGHTS_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				const std::string enStr  = Const::UNI_SPOT_LIGHTS_ENABLED_PREFIX + std::to_string(i) + Const::STR_INDEX_CLOSE;
				if (i < spots.size())
				{
					SetSpotLight(*prog, idxStr.c_str(), *spots[i],
					             spots[i]->GetWorldPosition(),
					             spots[i]->GetWorldDirection());
					Uniforms::SetInt(*prog, enStr.c_str(), 1);
				}
				else
				{
					Uniforms::SetInt(*prog, enStr.c_str(), 0);
				}
			}
		}
	}

} // namespace SJH
