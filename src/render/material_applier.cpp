#include "render/material_applier.h"
#include "material/material.h"
#include "program/program.h"
#include "program/program_uniforms.h"
#include "render/render_context.h"
#include "resource_registry/texture.h"

namespace SJH::MaterialApplier
{
    void WriteUniforms(const Program& prog, const Material& material)
    {
        Uniforms::SetInt  (prog, "material.diffuse",   material.GetDiffuseUnit());
        Uniforms::SetInt  (prog, "material.specular",  material.GetSpecularUnit());
        Uniforms::SetFloat(prog, "material.shininess", material.GetShininess());
    }

    void BindTextures(RenderContext& rc, const Material& material)
    {
        // GLint (Material 의 unit) → GLuint (RenderContext::BindTexture) 명시 캐스트.
        // sampler unit 은 음수일 수 없으나 Material 이 GLint 보관 (sampler location convention).
        if (auto* tex = material.GetDiffuseTexture())
            rc.BindTexture(static_cast<GLuint>(material.GetDiffuseUnit()), tex->GetTextureID());
        if (auto* tex = material.GetSpecularTexture())
            rc.BindTexture(static_cast<GLuint>(material.GetSpecularUnit()), tex->GetTextureID());
    }
}
