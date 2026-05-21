/**
 * @file material_applier.h
 * @brief Material 의 properties bag -> Program UniformCache 교집합 GL 송신 + 텍스처 바인딩.
 *
 * @details
 *  ### SP6 책임 (단일화)
 *  - SP1~SP5: WriteUniforms(prog, mat) + BindTextures(rc, mat) 2 메서드 (typed API 사용).
 *  - SP6: 단일 `Apply(rc, mat)` — properties bag iterate + UniformCache 교집합 -> GL 송신.
 *
 *  ### Unity 매핑
 *  Apply 가 Unity `material.SetPass(0)` 의 *셰이더에 properties 송신 + 텍스처 바인딩* 부분.
 *  셰이더가 받지 않는 properties 는 silent skip (UniformCache 미존재 entry).
 *
 *  ### 호출 전제 (POLA)
 *  - `mat.GetProgram() != nullptr` — Material 이 program 부착됨.
 *  - mat.GetCache() != nullptr — Program 의 cache reference (SetProgram 시 자동).
 *  - `rc.UseProgram(*mat.GetProgram())` 가 Apply 전에 호출 (RenderQueue.Flush 가 책임).
 */
#ifndef __SJH_MATERIAL_APPLIER_H__
#define __SJH_MATERIAL_APPLIER_H__

namespace SJH
{
    class Material;
    class RenderContext;

    namespace MaterialApplier
    {
        /// @brief Material properties bag -> UniformCache 교집합 GL 송신 + 텍스처 바인딩 일괄.
        /// @note  mat.GetProgram() 이 nullptr 이면 no-op (Observer cascade 후 안전).
        void Apply(RenderContext& rc, const Material& mat);
    }
}

#endif // __SJH_MATERIAL_APPLIER_H__
