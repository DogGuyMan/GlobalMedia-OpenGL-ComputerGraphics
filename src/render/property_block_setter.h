/**
 * @file property_block_setter.h
 * @brief @c MaterialPropertyBlock -> Program UniformCache 교집합 GL 송신 + 텍스처 바인딩.
 *
 * @details
 *  ### 책임 (SP-PropertyBlockSetter)
 *  - **입력**: @c MaterialPropertyBlock (값 컨테이너) + @c Program (셰이더 schema 출처).
 *  - **출력**: @c glUniform* (typed 값 송신) + @c glBindTexture (sampler 바인딩).
 *  - **알고리즘**: Cache outer + Type dispatch + Block lookup inner - Unity reflection 정통.
 *
 *  ### 비-책임
 *  - [X] GL pipeline state (depth/blend/stencil) 전환 -> @c DeviceContext::ApplyPipelineState 담당 (D-RS-1).
 *  - [X] Program 활성화 (@c glUseProgram) -> @c DeviceContext 담당.
 *  - [X] Pass.Kind / QueueLayer 해석 -> @c MeshPassProcessor 담당.
 *
 *  ### GL_BOOL uniform 처리 주의
 *  GLSL @c bool uniform 은 @c glUniform1i(0/1) 로 송신해야 하며,
 *  @c MaterialPropertyBlock 의 @c Ints 맵에 저장된다 (@c SetInt 로 0/1 값 저장).
 *  @c GL_BOOL case 는 @c GL_INT 와 fall-through 처리 - 삭제하면 bool uniform 이 영영 미업로드됨.
 *  (근본원인: @c uEnableHit / @c uEnableDissolve 등 sprite FX bool uniform 무반응 버그 선례 - 2026-06-XX)
 *
 *  ### 정통 매핑
 *  - Unity @c MaterialPropertyBlock + @c Renderer.SetPropertyBlock(block) 의 내부 적용 단계.
 *  - DX12 @c SetGraphicsRootDescriptorTable (uniform + texture 바인딩).
 *
 *  ### 시그니처 진화 이력
 *  - 이전: `Apply(rc, Material)` - Material 통째.
 *  - 현재: `Set(rc, MaterialPropertyBlock, Program)` - block 직접 (SRP, Setter 형제 일관성).
 *
 * @note namespace 자유 함수 형태 - 상태 없음. 매 호출이 독립적 (재진입 안전).
 */
#ifndef __SJH_PROPERTY_BLOCK_SETTER_H__
#define __SJH_PROPERTY_BLOCK_SETTER_H__

#include "material/material_property_block.h"
#include "device_context.h"
namespace SJH
{
	/**
	 * @brief @c MaterialPropertyBlock -> Program GL 송신 Applier - 자유 함수 네임스페이스.
	 * @details
	 *  Cache outer + Type dispatch + Block lookup inner 알고리즘.
	 *  Program 의 UniformCache 가 비어 있으면 no-op (Observer cascade 후 안전).
	 */
	namespace PropertyBlockSetter
	{
		/// @brief @c block 의 각 값을 @c prog 의 UniformCache 스키마에 맞게 GL 에 송신.
		/// @details
		///  - Cache outer: @c prog.GetUniformCache().Entries() 순회 (셰이더 schema 가 진실의 원천).
		///  - Type dispatch: @c entry.Type 으로 @c block 의 typed map (@c Floats / @c Ints / @c Vec3s 등) 분기.
		///  - Block lookup: @c find - 없으면 silent skip.
		///  - @c GL_SAMPLER_2D / @c GL_SAMPLER_CUBE: @c rc.BindTexture 로 텍스처 유닛 바인딩.
		///  - @c GL_BOOL: @c GL_INT 와 fall-through - @c Uniforms::SetInt(0/1) 로 송신
		///    (@c Ints 맵에서 조회, 삭제 금지 - bool uniform 무반응 버그 선례).
		/// @param rc    DeviceContext (BindTexture 위임).
		/// @param block 송신할 uniform/texture 값 컨테이너.
		/// @param prog  UniformCache schema 출처 + location 조회 대상.
		void Set(DeviceContext &rc, const MaterialPropertyBlock &block, const Program &prog);
	} // namespace PropertyBlockSetter
} // namespace SJH

#endif // __SJH_PROPERTY_BLOCK_SETTER_H__
