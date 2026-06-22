/**
 * @file light_ubo_uploader.h
 * @brief 수집된 Light 데이터를 (1) per-frame 공유 LightBlock UBO 로 패킹 + (2) 등록된 Program 들에 전달.
 *
 * @details
 *  ### 존재 의의 + O4 승격 (2026-06-21, D-LUD)
 *  구 @c LightUniformDispatcher (stateless loose-only 송신) 를 *UBO owner 로 승격* 한 결과
 *  (재검토 워크플로우 @c wf_25f028c9). 3 엔진 must-have (Unity @c ForwardLights / Unreal
 *  @c FLightShaderParameters / Godot @c uniform_set) = "광원 데이터를 *공유 버퍼* 에 1회 업로드".
 *  본 클래스가 그 공유 LightBlock UBO 를 소유한다 (stateful).
 *
 *  ### UBO 경로 (Phase C - loose 경로 폐지, UBO 전용)
 *  - Slang phong UBO 셰이더: @ref Update 가 std140 LightBlock 1회 패킹 + @ref BindTo 가
 *    각 program 의 LightBlock 소유권을 회수(@c Program::DisownUniformBlock)한 뒤 공유 UBO 를 결속.
 *  - (구 loose glUniform* 경로는 Phase C 에서 제거 - 전 lit 셰이더가 LightBlock UBO 라 소비자 0.)
 *
 *  ### 책임 분할 (D-LUD-2 - Dispatch -> Update / BindTo)
 *  - @ref Update : per-frame 1회 - 광원 -> std140 LightBlock UBO 패킹.
 *  - @ref BindTo : program 순회 - LightBlock 보유 program 에 공유 UBO 결속 (그 외 skip).
 *
 *  ### 비-책임
 *  - [X] Light 컴포넌트 수집 (SceneContext) -> @c SceneRenderer::RenderWithCamera 담당.
 *  - [X] 거리 감쇠 계수 계산 -> @c GetAttenuationCoeff (@c object/light.h) 담당.
 *
 *  ### 모듈 경계 (D6 유지)
 *  광원 struct -> std140/uniform 변환 지식(cutoff degree->cosine, attenuation)을 @c .cpp 익명 ns 에 가둬
 *  render -> object/scene 역의존을 차단 (render_passable TU 가 광원 타입 세부를 직접 알지 않게).
 *
 * @note 다중 phong 셰이더 hardening 보류 (YAGNI): 현재 phong 셰이더 1종이라 LightBlock 의 introspected
 *       binding point 가 안정적. 2종 이상이면 reserved binding point + @c glUniformBlockBinding 재배치 필요.
 */
#ifndef __SJH_LIGHT_UBO_UPLOADER_H__
#define __SJH_LIGHT_UBO_UPLOADER_H__

// 헤더는 GL-free 유지 (modular-build-discipline) - UniformBuffer 를 forward-decl 하고
// unique_ptr 멤버 + out-of-line 소멸자로 다룬다. uniform_buffer.h(-> gl3w.h) 를 헤더에서 끌어오면
// Effekseer 의 macOS gl3.h 와 gl.h+gl3.h 충돌(PFNGLGETPOINTERVPROC)을 일으키므로 .cpp 에만 include.
#include <memory>
#include <vector>
#include <glm/glm.hpp>

namespace SJH
{
	class Program;
	class UniformBuffer;
	class DirLight;
	class PointLight;
	class SpotLight;

	/**
	 * @brief 수집된 Light 를 per-frame 공유 LightBlock UBO 로 패킹 + LightBlock 보유 program 에 결속.
	 * @details O4 승격 (D-LUD) - 구 @c LightUniformDispatcher 의 UBO owner 화. 자세한 2 경로 공존은 파일 헤더 참조.
	 */
	class LightUboUploader
	{
	  public:
		LightUboUploader();
		/// @brief out-of-line - @c mLightBlockUbo 가 incomplete @c UniformBuffer 의 unique_ptr 이라 .cpp 에서 정의.
		~LightUboUploader();

		/// @brief per-frame 1회 - 수집된 광원을 std140 LightBlock UBO 로 패킹 (공유 UBO 에 업로드).
		/// @param dir     활성 DirLight 포인터. nullptr 이면 @c dirLightEnabled = 0.
		/// @param points  활성 PointLight 목록 (@c MAX_POINT_LIGHTS 초과분 warn + 무시).
		/// @param spots   활성 SpotLight 목록 (@c MAX_SPOT_LIGHTS 초과분 warn + 무시).
		/// @param viewPos 카메라 월드 위치 (Phong specular). std140 LightBlock 의 viewPos 멤버.
		void Update(DirLight* dir,
		            const std::vector<PointLight*>& points,
		            const std::vector<SpotLight*>& spots,
		            const glm::vec3& viewPos);

		/// @brief @ref Update 직후 - @p programs 에 전달.
		/// @param programs @c ResourceRegistry::GetAllPrograms() 스냅샷 (외부 push - D-1).
		/// @details LightBlock 보유 program = 공유 UBO 결속 (소유권 회수 후 BindBase). 그 외(simple/skybox/postfx) = skip.
		void BindTo(const std::vector<Program*>& programs);

	  private:
		/// @brief per-frame 공유 LightBlock UBO (lazy create, O4 owner). std140 size = sizeof(LightBlockStd140).
		///        UniformBuffer 는 forward-decl (헤더 GL-free) - .cpp 에서 complete type 으로 다룬다.
		///        Phase C (D-DPP-5) - loose 경로 제거로 구 프레임 캐시(mDirPtr/mPoints/mSpots/mViewPos) 삭제.
		std::unique_ptr<UniformBuffer> mLightBlockUbo;
	};

} // namespace SJH

#endif // __SJH_LIGHT_UBO_UPLOADER_H__
