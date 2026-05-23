#ifndef __SJH_LAYER_H__
#define __SJH_LAYER_H__

#include <cstdint>
#include <cstddef>

namespace SJH
{
	/// @brief 0-based index → 독립 bit 슬롯. reservedBits 만큼 하위 bit 를 건너뜀.
	///   Unity LayerMask / Cocos CameraMask 정통 패턴.
	///   예: LayerBit(0,0)=bit0=1, LayerBit(0,1)=bit1=2, LayerBit(1,1)=bit2=4
	constexpr uint32_t LayerBit(std::size_t index, std::size_t reservedBits = 1)
	{
		return static_cast<uint32_t>(std::size_t{1} << (index + reservedBits));
	}

	/// @brief 씬 기본 레이어 (bit 0). SceneCamera 는 이 비트만 culling mask 로 가짐.
	constexpr uint32_t LAYER_SCENE = 1u;
} // namespace SJH

#endif // __SJH_LAYER_H__
