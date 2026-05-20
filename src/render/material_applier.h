#ifndef __SJH_MATERIAL_APPLIER_H__
#define __SJH_MATERIAL_APPLIER_H__

namespace SJH
{
    class Program;
    class Material;
    class RenderContext;

    /// @brief Material 의 uniform/texture 적용을 *순수 데이터 → GL 호출* 로 분리하는 helper.
    /// @details Material 은 순수 데이터 (D-1). MaterialApplier 가 그 데이터를 읽어
    ///          Uniforms::Set* 와 RenderContext::BindTexture 발행 (Q4-3 분리).
    namespace MaterialApplier
    {
        /// @brief Material 의 sampler unit (diffuse/specular) + shininess uniform 송신.
        /// @note  prog 가 미리 bound (UseProgram) 되어 있어야 함 — POLA 호출자 책임.
        void WriteUniforms(const Program& prog, const Material& material);

        /// @brief diffuse/specular 텍스처를 sampler unit 에 바인딩.
        void BindTextures(RenderContext& rc, const Material& material);
    }
}

#endif // __SJH_MATERIAL_APPLIER_H__
