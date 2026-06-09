/**
 * @file layer.h
 * @brief Actor 가시성 비트마스크 - @c Layer enum class + 비트 연산자 + @c ToBits 변환.
 *
 * @details
 *  ### 책임
 *  - Unity @c LayerMask 정통의 64비트 @c Layer enum class 정의.
 *  - @c operator| (OR 결합) / @c operator& (AND 검사) / @c ToBits 변환 제공.
 *  - @c Actor::SetLayer / @c Camera::CullingMask 비트 AND 검사의 타입 안전 약속 계층.
 *
 *  ### 비-책임
 *  - [X] 렌더 필터 실행 - @c SceneRenderer 가 @c Actor::GetLayer() & @c Camera::CullingMask 검사.
 *  - [X] 물리 레이어 - Box2D @c PhysicsLayer(@c uint64_t) 는 별도 파일.
 *
 * @note 새 비트 추가 시 본 파일에 *모든 비트 자리 검토 후* 진입 (64비트 = Unity 32비트보다 풍부).
 */

#ifndef __SJH_SCENE_LAYER_H__
#define __SJH_SCENE_LAYER_H__

#include <cstdint>

namespace SJH::Scene
{
	/// @brief Actor 가시성 비트 약속 - Unity LayerMask 정통, FSM uint64 enum 컨벤션과 일관.
	/// @details Actor.SetLayer(Layer) 의 비트 + Camera.CullingMask 의 AND 검사.
	///          uint64  64 비트 (Unity 32 보다 풍부). FSM StateMachine 의 enum class 비트 패턴과 동일.
	///          새 비트 추가 시 본 파일에 *모든 비트 자리 검토 후* 진입.
	enum class Layer : uint64_t
	{
		Default   = 1ull << 0,   ///< 1  - 모든 Actor 기본
		Player    = 1ull << 1,   ///< 2  - 미래 게임 로직 (예약)
		Enemy     = 1ull << 2,   ///< 4  - 미래 게임 로직 (예약)
		UI        = 1ull << 3,   ///< 8  - 본 SP 가 자리 정의 (미래 UI Camera 진입자가 사용)
		DebugDraw = 1ull << 4,   ///< 16 - 미래 DebugDrawStage (예약)
		Screen    = 1ull << 5,   ///< 32 - PassComponent Actor 전용 (ScreenCamera 전용 레이어)

		All       = ~0ull,       ///< Camera 기본 mask - 모든 layer 매치
	};

	/// @brief 비트 OR - 여러 layer 결합 (예: @c Layer::Default | @c Layer::UI).
	/// @return 두 layer 의 비트 합집합을 갖는 @c Layer 값.
	constexpr Layer operator|(Layer a, Layer b) noexcept
	{
		return static_cast<Layer>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
	}

	/// @brief 비트 AND - cullingMask 검사. 결과 @c uint64_t (0 이면 매치 안 됨).
	/// @details @c Camera::CullingMask & @c actor.GetLayer() 형태로 SceneRenderer 가 사용.
	constexpr uint64_t operator&(Layer a, Layer b) noexcept
	{
		return static_cast<uint64_t>(a) & static_cast<uint64_t>(b);
	}

	/// @brief @c Layer -> @c uint64_t 명시 변환 (호환 호출용).
	/// @details @c Actor::SetLayer(uint64_t) 또는 @c Camera::CullingMask 직접 대입 시 사용.
	constexpr uint64_t ToBits(Layer l) noexcept { return static_cast<uint64_t>(l); }
}

#endif // __SJH_SCENE_LAYER_H__
