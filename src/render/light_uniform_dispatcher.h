/**
 * @file light_uniform_dispatcher.h
 * @brief 활성 Light 컴포넌트 데이터를 등록된 모든 Program 에 일괄 uniform 송신.
 *
 * @details
 *  ### 존재 의의 - SceneRenderer 에서 Light uniform 송신 책임 분리 (Phase 2)
 *  - @c SceneRenderer 의 책임: Camera 수집/순회 + 위임.
 *  - @c LightUniformDispatcher 의 책임: Program 순회 + uniform 값 쓰기.
 *  - *구조적* 동기(가독성보다 강함): 광원 struct->uniform 변환 지식(cutoff degree->cosine,
 *    @c GetAttenuationCoeff)을 .cpp 익명 ns 에 가둬 render -> object/scene 역의존을 차단 (D6, 2026-06-11).
 *    -> render_stage TU 가 광원 타입 세부를 직접 알지 않게 하는 모듈 경계 방어막.
 *
 *  ### 비-책임
 *  - [X] Light 컴포넌트 수집 (SceneContext) -> @c SceneRenderer::RenderWithCamera 담당.
 *  - [X] Program 활성화 (@c glUseProgram) 선택 -> 내부에서 lighting sentinel(@c UNI_VIEW_POS) 확인 후
 *    직접 @c DeviceContext::UseProgram 호출 (라이팅 uniform 송신 전 program 활성화 필수).
 *
 *  ### Lighting sentinel
 *  @c UNI_VIEW_POS uniform 이 UniformCache 에 없는 Program 은 lighting 미사용
 *  (simple/passthrough/postfx 셰이더 등) 으로 판정하여 uniform 송신 전체 skip -
 *  warn-once 노이즈 차단 + @c glUseProgram 비용 회피.
 *
 *  ### 현재 구현 vs 목표 (정통성 주의 - 2026-06-21 재검증)
 *  *현재* 본 클래스는 program 들을 순회하며 각 program 에 loose @c glUniform* 로 light 값을 *반복 송신*한다.
 *  이는 아래 3 엔진 정통과 *반대* 다 - 정통은 "프레임당 공유 버퍼 1회 업로드 + program 순회 0" 이다.
 *  Slang UBO 마이그레이션(Phase 2.5, per-frame 공유 LightBlock UBO)에서 본 클래스를 UBO owner 로
 *  승격하면 비로소 아래 정통에 정렬된다 (loose 경로는 비-UBO 셰이더용으로 공존 보존).
 *
 *  ### 3 엔진 정통 - light 데이터 GPU 도달 경로 (Context7 재검증 2026-06-21)
 *  - Unity URP : @c ForwardLights 가 프레임당 전역 CBUFFER 1회 업로드 -> 셰이더가 @c GetMainLight /
 *    @c GetAdditionalLight 로 pull. (구 주석의 "LightLoop" 은 HDRP 용어 - URP CPU 클래스는 ForwardLights)
 *  - Unreal    : @c FLightSceneProxy::GetLightShaderParameters 가 @c FLightShaderParameters 로 패킹 ->
 *    RDG uniform buffer 1회 바인딩. (구 주석의 @c FDeferredLightUniformStruct 는 공개 문서 미확인)
 *  - Godot 4   : @c RenderingDevice (@c uniform_buffer_create + @c uniform_set_create) 가 공유 버퍼로
 *    1회 바인딩 (@c RenderingServer 는 파사드 - 실 업로드는 RD 3 계층). 셰이더 @c light() built-in 소비.
 *  -> 공통 must-have: Light 컴포넌트는 *데이터 보유만*, GPU 전송은 외부 시스템이 *공유 버퍼 1회* 로 전담.
 *
 * @note 상태 없음 - 매 @c Dispatch 호출이 독립적 (동일 인스턴스 재사용 안전).
 *       (UBO owner 승격 시 UniformBuffer 1개를 멤버로 소유하는 stateful 로 전환 예정 - Phase 2.5.)
 */
#ifndef __SJH_LIGHT_UNIFORM_DISPATCHER_H__
#define __SJH_LIGHT_UNIFORM_DISPATCHER_H__

#include <vector>
#include <vmath.h>

namespace SJH
{
	class Program;
	class DirLight;
	class PointLight;
	class SpotLight;

	/**
	 * @brief 수집된 Light 목록을 등록된 모든 Program 에 일괄 uniform 송신.
	 * @details
	 *  @c SceneRenderer::RenderWithCamera 가 SceneContext 에서 Light 를 수집한 뒤 위임.
	 *
	 *  송신 항목:
	 *  - @c viewPos - Phong specular 계산용 카메라 월드 위치.
	 *  - DirLight 1개 - 없으면 @c dirLightEnabled = 0 만 전송.
	 *  - PointLight 배열 - 최대 @c MAX_POINT_LIGHTS 개, 부족하면 enabled = 0 으로 slot 채움.
	 *  - SpotLight 배열  - 최대 @c MAX_SPOT_LIGHTS 개, PointLight 와 동일 패턴.
	 */
	class LightUniformDispatcher
	{
	  public:
		/// @brief 수집된 Light 세트를 @p programs 전체에 uniform 송신.
		/// @details
		///  각 Program 에 대해 @c UNI_VIEW_POS lighting sentinel 확인 후 송신:
		///  1. viewPos (@c Phong specular).
		///  2. DirLight - 있으면 @c Uniforms::SetDirLight + enabled=1, 없으면 enabled=0.
		///  3. PointLight 배열 - 최대 @c MAX_POINT_LIGHTS, 초과분은 warn+무시.
		///  4. SpotLight 배열  - 최대 @c MAX_SPOT_LIGHTS, PointLight 와 동일 패턴.
		/// @param programs @c ResourceRegistry::GetAllPrograms() 스냅샷.
		/// @param dir      활성 DirLight 포인터. nullptr 이면 @c dirLightEnabled = 0 전송.
		/// @param points   활성 PointLight 목록 (@c MAX_POINT_LIGHTS 초과분 warn+무시).
		/// @param spots    활성 SpotLight 목록 (@c MAX_SPOT_LIGHTS 초과분 warn+무시).
		/// @param viewPos  카메라 월드 위치 (Phong specular @c viewPos uniform).
		/// @note lighting sentinel: @c UNI_VIEW_POS 가 없는 program 은 lighting 미사용으로 판정해 skip.
		void Dispatch(const std::vector<Program *> &programs,
		              DirLight *dir,
		              const std::vector<PointLight *> &points,
		              const std::vector<SpotLight *> &spots,
		              const vmath::vec3 &viewPos);
	};

} // namespace SJH

#endif // __SJH_LIGHT_UNIFORM_DISPATCHER_H__
