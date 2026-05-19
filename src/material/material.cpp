/**
 * @file material.cpp
 * @brief @c Material::Apply 정의 — Program/Texture 의 완전 정의가 필요한 유일한 코드.
 *
 * @details
 *  헤더(@c material.h)는 @c Program / @c Texture 를 전방선언만 한다. @c Apply 는
 *  @c Texture::GetTextureID() (inline getter) 와 @c Uniforms::Set* 만 호출하므로,
 *  본 TU 는 @c SJH::program 만 link 하면 된다 (@c resource_registry link 불필요 —
 *  out-of-line @c Texture 멤버는 호출 안 함). 이로써 모듈 의존이 단방향 DAG 로 유지된다.
 */
#include "material/material.h"
#include "program/program_uniforms.h"
#include "common/constants.h"
#include "resource_registry/texture.h" // Texture 완전 정의 — GetTextureID() inline 호출

namespace SJH
{
    void Material::Apply() const
    {
        if (mProgram == nullptr)
            return;

        if (mDiffuseTexture)
        {
            glActiveTexture(GL_TEXTURE0 + mDiffuseUnit);
            glBindTexture(GL_TEXTURE_2D, mDiffuseTexture->GetTextureID());
            Uniforms::SetInt(*mProgram, Const::UNI_MATERIAL_DIFFUSE, mDiffuseUnit);
        }
        if (mSpecularTexture)
        {
            glActiveTexture(GL_TEXTURE0 + mSpecularUnit);
            glBindTexture(GL_TEXTURE_2D, mSpecularTexture->GetTextureID());
            Uniforms::SetInt(*mProgram, Const::UNI_MATERIAL_SPECULAR, mSpecularUnit);
        }
        Uniforms::SetFloat(*mProgram, Const::UNI_MATERIAL_SHININESS, mShininess);
    }
}
