/**
 * @file render_passable.h
 * @brief 최상위 렌더 단계 순수 추상 IPassable - 다중 렌더 pass 의 직교 분리.
 *
 * @details
 *  ### 책임
 *  - Application 이 매 프레임 호출하는 *순차적 렌더 단계* 의 공통 인터페이스 제공.
 *  - 각 Pass 는 *직교 책임* (Scene / PostFX / ImGui / Skybox / DebugDraw 등) 을 충족하는
 *    자유도를 가지며, 내부적으로 Pass 를 Queue 분류하기 위해 @c Material::Pass::RenderQueue 를 활용.
 *    Pass(IPassable) != Queue - Queue 는 한 Pass *내부* 분류 단위.
 *
 *  ### 계약 (D2)
 *  - @c Draw(rec, before_frame_buffer_texture) - rec=GL facade, before_frame_buffer_texture=직전 Pass 결과 텍스처(체이닝용, 미사용 시 nullptr).
 *  - @c GetPassResult() - 이 Pass 의 출력 텍스처(다음 Pass 의 before_frame_buffer_texture 로 연결). 없으면 nullptr.
 *  - @c BeforeIndex - 입력으로 삼을 선행 Pass 인덱스(-1=없음). PassIterator 가 해석.
 *
 *  ### 비-책임
 *  - [X] Pass 생성/소유 - Application 의 stage 컬렉션 책임.
 *
 * @note 최상위 추상이라 render_passable/ 폴더에 *파일 그대로* 유지 (impls 로 병합 안 함).
 *       vtable 홈 TU 는 같은 폴더의 render_passable.cpp (header-only abstract ODR 보장).
 *       GL 격리: 이 헤더는 GL 헤더를 include 하지 않는다 (Texture/DeviceContext 전방선언만).
 */
#include <string>
#ifndef __SJH_I_PASSABLE_H__
#define __SJH_I_PASSABLE_H__

namespace SJH
{
	class Texture;
	class DeviceContext;

	/**
	 * @brief 최상위 렌더 단계 순수 추상 - Unity ScriptableRenderPass / Unreal FSceneRenderer 정통.
	 * @details
	 *  Application 의 @c render() 가 보유한 @c IPassable* 컬렉션을 순서대로 호출.
	 *  각 pass 는 *직교 책임* - Scene / PostFX / ImGui / Skybox / DebugDraw 등.
	 *
	 *  ### 구체 클래스 목록 (render_passable.impls.{h,cpp})
	 *  | 클래스 | 책임 |
	 *  |---|---|
	 *  | @ref SceneRenderer | Actor 트리 순회 + IRenderable 큐 Flush (orchestrator) |
	 *  | @ref CameraStage | 단일 Camera 의 씬 렌더 위임 |
	 *  | @ref ScreenQuadStage | FBO -> backbuffer 합성 (PostFX 최종 출력) |
	 */
	class IPassable
	{
	  public:
		virtual std::string GetPassKey() const = 0;
		virtual ~IPassable() = default;

		/// @brief 매 프레임 호출 - GL facade 와 선행 Pass 결과 텍스처를 받아 자기 출력을 그린다.
		/// @param rec    GL 파이프라인 facade (DeviceContext 싱글톤).
		/// @param before_frame_buffer_texture 직전(BeforeIndex) Pass 의 결과 텍스처. 없으면 nullptr.
		virtual void Draw(DeviceContext &rec, const Texture *before_frame_buffer_texture) = 0;

		/// @brief 이 Pass 의 출력 텍스처 - 다음 Pass 의 @p before_frame_buffer_texture 입력으로 연결 (D6). 없으면 nullptr.
		virtual const Texture *GetPassResult() const = 0;

		/// @brief Window resize broadcast - 내부 FBO 크기 sync 용. 기본 no-op.
		/// @param w 새 창 가로 크기 (픽셀).
		/// @param h 새 창 세로 크기 (픽셀).
		virtual void OnResize(int /*w*/, int /*h*/) {}

		/// @brief 입력으로 삼을 선행 Pass 인덱스. -1=없음(scene raw). PassIterator 가 해석.
		int BeforeIndex = -1;

		/// @brief false 면 PassIterator 가 이 Pass 를 skip(동적 비활성, design B). 체인은 자동 재연결.
		bool Enabled = true;
	};
} // namespace SJH

#endif // __SJH_I_PASSABLE_H__
