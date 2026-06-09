/**
 * @file layer.h
 * @brief @c SJH 네임스페이스 레이어 유틸리티 - 비트 슬롯 생성 헬퍼 + 레거시 @c LAYER_SCENE 호환 alias.
 *
 * @details
 *  ### 책임
 *  - @ref SJH::LayerBit - 0-based 인덱스를 독립 비트 슬롯(@c uint32_t)으로 변환.
 *    Unity @c LayerMask / Cocos @c CameraMask 정통 패턴.
 *  - @c LAYER_SCENE - @c SJH::Scene::Layer::Default 의 하위 호환 alias
 *    (옛 데모 @c migrate_demo / @c effekseer_demo 가 사용하던 이름).
 *
 *  ### 비-책임
 *  - [X] 레이어 비트 집합 정의 - 정본은 @c scene/layer.h 의 @c SJH::Scene::Layer enum.
 *  - [X] Physics 레이어 비트 - @c apps/_MyApp_/src/Physics/PhysicsLayer.h 담당.
 *
 * @note @c LAYER_SCENE 은 @deprecated - 신규 코드는 @c SJH::Scene::Layer::Default 를 직접 사용.
 *       SP-LayerMigration 데모 마이그레이션 완료 후 본 파일 삭제 예정.
 */
#ifndef __SJH_LAYER_H__
#define __SJH_LAYER_H__

#include <cstdint>
#include <cstddef>

#include "scene/layer.h"

namespace SJH
{
	/**
	 * @brief 0-based 인덱스를 독립 비트 슬롯(@c uint32_t)으로 변환.
	 * @details Unity @c LayerMask / Cocos @c CameraMask 정통 패턴.
	 *          @p reservedBits 만큼 하위 비트를 건너뛰어 슬롯을 배치한다.
	 *
	 *  예:
	 *  - @c LayerBit(0, 0) = bit0 = 1
	 *  - @c LayerBit(0, 1) = bit1 = 2
	 *  - @c LayerBit(1, 1) = bit2 = 4
	 *
	 * @param index        0-based 레이어 인덱스.
	 * @param reservedBits 하위 예약 비트 수 (기본 1 - bit0 는 @c Scene::Layer::Default 가 사용).
	 * @return 해당 슬롯의 비트마스크.
	 */
	constexpr uint32_t LayerBit(std::size_t index, std::size_t reservedBits = 1)
	{
		return static_cast<uint32_t>(std::size_t{1} << (index + reservedBits));
	}

	/**
	 * @brief @c SJH::Scene::Layer::Default 의 레거시 하위 호환 alias.
	 * @deprecated SP5 Task 7 - 신규 코드는 @c SJH::Scene::Layer::Default 를 직접 사용.
	 *             옛 호출처(@c migrate_demo, @c effekseer_demo) 호환 유지용.
	 *             SP-LayerMigration 데모 마이그레이션 완료 후 본 상수 및 파일 삭제 예정.
	 */
	constexpr uint64_t LAYER_SCENE = static_cast<uint64_t>(SJH::Scene::Layer::Default);
} // namespace SJH

#endif // __SJH_LAYER_H__
