#ifndef __SJH_LAYER_H__
#define __SJH_LAYER_H__

#include <cstdint>
#include <cstddef>

#include "scene/layer.h"

namespace SJH
{
	/// @brief 0-based index -> 독립 bit 슬롯. reservedBits 만큼 하위 bit 를 건너뜀.
	///   Unity LayerMask / Cocos CameraMask 정통 패턴.
	///   예: LayerBit(0,0)=bit0=1, LayerBit(0,1)=bit1=2, LayerBit(1,1)=bit2=4
	constexpr uint32_t LayerBit(std::size_t index, std::size_t reservedBits = 1)
	{
		return static_cast<uint32_t>(std::size_t{1} << (index + reservedBits));
	}

	/// @deprecated SP5 Task 7 — `SJH::Scene::Layer::Default` 사용 권장.
	///             옛 호출처 (migrate_demo, effekseer_demo) 호환 유지용 alias.
	///             별 SP (SP-LayerMigration) 에서 데모 마이그레이션 후 본 파일 삭제 예정.
	constexpr uint64_t LAYER_SCENE = static_cast<uint64_t>(SJH::Scene::Layer::Default);
} // namespace SJH

#endif // __SJH_LAYER_H__
