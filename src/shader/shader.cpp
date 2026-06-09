/**
 * @file shader.cpp
 * @brief Shader - OpenGL 셰이더 객체 RAII 래퍼 구현.
 *
 * @details
 *  ### 책임
 *  - @c CreateFromFile : @c sb7::shader::load 경유 파일 IO + 컴파일 + @c TryLoadFile 진단 호출.
 *  - @c CreateFromSource : 인라인 소스에서 @c glCreateShader + @c glShaderSource +
 *    @c glCompileShader 직접 수행 후 @c GLObjectLog::CheckShaderCompile 진단.
 *  - RAII 불변식 컴파일 타임 검증 (@c static_assert - SP1).
 *
 *  ### 비-책임
 *  - [X] 프로그램 링킹 (@c glAttachShader / @c glLinkProgram) - @c SJH::Program 담당.
 *
 * @note @c sb7::shader::load 는 @c extern/sb7code 제공 - @c LoadTextFile 폐기 후 일괄 경유.
 *       @c CreateFromSource 는 파일 경로 tag 없이 진단하므로, spdlog 출력에 파일명이 비어 있다.
 */
#include "shader/shader.h"
#include "diagnostics/gl_log.h"
#include <memory>
#include <shader.h>     // sb7::shader::load - 파일 -> GLuint, sb7code 제공
#include <type_traits>

// SP1 - RAII 의미론 컴파일 타임 검증. glDeleteShader 이중 호출 위험 차단.
static_assert(!std::is_copy_constructible_v<SJH::Shader>,
              "SJH::Shader must be non-copy-constructible (RAII)");
static_assert(!std::is_copy_assignable_v<SJH::Shader>,
              "SJH::Shader must be non-copy-assignable (RAII)");
static_assert(!std::is_move_constructible_v<SJH::Shader>,
              "SJH::Shader must be non-move-constructible (factory + UPtr only)");
static_assert(!std::is_move_assignable_v<SJH::Shader>,
              "SJH::Shader must be non-move-assignable (factory + UPtr only)");

namespace SJH
{
    ShaderUPtr Shader::CreateFromFile(const std::string &filename, GLenum shader_type)
    {
        // private 생성자도 클래스 자신의 static 멤버에서는 호출 가능 - 팩토리 패턴의 핵심
        auto shader = std::unique_ptr<Shader>(new Shader());
        if (!shader->TryLoadFile(filename, shader_type))
            return nullptr;
        return shader;
    }

    ShaderUPtr Shader::CreateFromSource(const std::string &source, GLenum shader_type)
    {
        auto shader = std::unique_ptr<Shader>(new Shader());

        const char *codePtr   = source.c_str();
        const GLint codeLength = static_cast<GLint>(source.length());

        shader->mShaderAddr = glCreateShader(shader_type);
        glShaderSource(shader->mShaderAddr, 1, &codePtr, &codeLength);
        glCompileShader(shader->mShaderAddr);

        // 인라인 소스라 파일 경로 tag 없음 - 진단은 빈 tag 로 호출 (default 메시지).
        if (!Diagnostics::GLObjectLog::CheckShaderCompile(shader->mShaderAddr, ""))
            return nullptr;
        return shader;
    }

    Shader::~Shader()
    {
        if(mShaderAddr != 0)
            glDeleteShader(mShaderAddr);
    }

    bool Shader::TryLoadFile(const std::string &filename, GLenum shader_type)
    {
        // common 의 LoadTextFile 폐기 - sb7::shader::load 가 파일 IO + glCreateShader +
        // glShaderSource + glCompileShader 까지 일괄 수행 (sb7code 제공).
        // 주의: 멤버 mShaderAddr 에 직접 대입 (지역 변수 shadow 금지).
        mShaderAddr = sb7::shader::load(filename.c_str(), shader_type, true);
        return Diagnostics::GLObjectLog::CheckShaderCompile(mShaderAddr, filename);
    }
}
