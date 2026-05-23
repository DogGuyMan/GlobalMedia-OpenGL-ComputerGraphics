/**
 * @file property_block_setter.h
 * @brief MaterialPropertyBlock -> Program UniformCache 교집합 GL 송신 + 텍스처 바인딩.
 *
 * @details
 *  ### 책임 (SP-PropertyBlockSetter — 옛 PropertyBlockSetter rename + 시그니처 정밀화)
 *  - **입력**: `MaterialPropertyBlock` (값) + `Program` (셰이더 schema 출처)
 *  - **출력**: `glUniform*` (값별 송신) + `glBindTexture` (sampler 바인딩)
 *  - **알고리즘**: Cache outer + Type dispatch + Block lookup inner — Unity reflection 정통
 *
 *  ### 정통 매핑
 *  - Unity `MaterialPropertyBlock` + `Renderer.SetPropertyBlock(block)` 의 *내부 적용* 단계
 *  - DX12 `SetGraphicsRootDescriptorTable` (uniform + texture 바인딩)
 *
 *  ### PropertyBlockSetter 와의 차이 (rename + 진화)
 *  - 이름: `PropertyBlockSetter` -> `PropertyBlockSetter` (Setter 형제 일관성 + Unity 어휘)
 *  - 시그니처: `(rc, Material)` -> `(rc, MaterialPropertyBlock, Program)` (block 직접 — 진정한 분리)
 *  - 메서드: `Apply` -> `Set` (Setter 형제 일관성)
 */
#ifndef __SJH_PROPERTY_BLOCK_SETTER_H__
#define __SJH_PROPERTY_BLOCK_SETTER_H__

#include "material/material_property_block.h"
#include "device_context.h"
namespace SJH
{

	namespace PropertyBlockSetter
	{
		/// @brief MaterialPropertyBlock -> Program 의 UniformCache 교집합 GL 송신 + 텍스처 바인딩.
		/// @details Cache outer + Type dispatch + Block lookup inner.
		///          Program 의 cache 가 nullptr 이면 no-op (Observer cascade 후 안전).
		void Set(DeviceContext &rc, const MaterialPropertyBlock &block, const Program &prog);
	} // namespace PropertyBlockSetter
} // namespace SJH

#endif // __SJH_PROPERTY_BLOCK_SETTER_H__
