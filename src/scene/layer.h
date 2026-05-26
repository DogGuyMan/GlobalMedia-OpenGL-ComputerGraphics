#ifndef __SJH_SCENE_LAYER_H__
#define __SJH_SCENE_LAYER_H__

#include <cstdint>

namespace SJH::Scene
{
	/// @brief Actor 가시성 비트 약속 — Unity LayerMask 정통, FSM uint64 enum 컨벤션과 일관.
	/// @details Actor.SetLayer(Layer) 의 비트 + Camera.CullingMask 의 AND 검사.
	///          uint64  64 비트 (Unity 32 보다 풍부). FSM StateMachine 의 enum class 비트 패턴과 동일.
	///          새 비트 추가 시 본 파일에 *모든 비트 자리 검토 후* 진입.
	enum class Layer : uint64_t
	{
		Default   = 1ull << 0,   ///< 1  — 모든 Actor 기본
		Player    = 1ull << 1,   ///< 2  — 미래 게임 로직 (예약)
		Enemy     = 1ull << 2,   ///< 4  — 미래 게임 로직 (예약)
		UI        = 1ull << 3,   ///< 8  — 본 SP 가 자리 정의 (미래 UI Camera 진입자가 사용)
		DebugDraw = 1ull << 4,   ///< 16 — 미래 DebugDrawStage (예약)
		Screen    = 1ull << 5,   ///< 32 — PassComponent Actor 전용 (ScreenCamera 전용 레이어)

		All       = ~0ull,       ///< Camera 기본 mask — 모든 layer 매치
	};

	/// @brief 비트 OR — 여러 layer 결합 (`Layer::Default | Layer::UI`).
	constexpr Layer operator|(Layer a, Layer b) noexcept
	{
		return static_cast<Layer>(static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
	}

	/// @brief 비트 AND — cullingMask 검사. 결과 uint64 (0 이면 매치 안 됨).
	constexpr uint64_t operator&(Layer a, Layer b) noexcept
	{
		return static_cast<uint64_t>(a) & static_cast<uint64_t>(b);
	}

	/// @brief Layer  uint64 명시 변환 (호환 호출용).
	constexpr uint64_t ToBits(Layer l) noexcept { return static_cast<uint64_t>(l); }
}

#endif // __SJH_SCENE_LAYER_H__
