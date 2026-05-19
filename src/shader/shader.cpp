#include "shader/shader.h"
#include "diagnostics/gl_log.h"
#include <memory>

namespace SJH
{
    ShaderUPtr Shader::CreateFromFile(const std::string &filename, GLenum shader_type)
    {
        // private 생성자도 클래스 자신의 static 멤버에서는 호출 가능 — 팩토리 패턴의 핵심
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

        // 인라인 소스라 파일 경로 tag 없음 — 진단은 빈 tag 로 호출 (default 메시지).
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
        auto result = LoadTextFile(filename);
        if (!result.has_value())
            return false;

        auto &code = result.value();
        const char *codePtr = code.c_str();
        GLint codeLength = (GLint)code.length();

        // OpenGL shader object 생성
        mShaderAddr = glCreateShader(shader_type);

        // shader에 소스 코드 설정
        glShaderSource(mShaderAddr, 1, &codePtr, &codeLength);

        // 셰이더 컴파일
        glCompileShader(mShaderAddr);
        bool isSuccess = Diagnostics::GLObjectLog::CheckShaderCompile(mShaderAddr, filename);
        return isSuccess;
    }
}
