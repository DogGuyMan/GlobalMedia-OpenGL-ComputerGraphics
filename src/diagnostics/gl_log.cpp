#include "gl_log.h"

#include <GL/glcorearb.h>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    namespace detail
    {
        // 프로그램별 1회 검사 캐시 — true 면 모든 기대 uniform/attribute 존재.
        std::unordered_map<GLuint, bool> uniformChecked;
        std::unordered_map<GLuint, bool> attribChecked;
    }
}

namespace SJH::Diagnostics
{
    // 내부 헬퍼 함수들
    namespace
    {

        std::string FetchShaderInfoLog(GLuint shader)
        {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return {};
            std::string log(static_cast<size_t>(length), '\0');
            glGetShaderInfoLog(shader, length, nullptr, log.data());
            if (!log.empty() && log.back() == '\0')
                log.pop_back();
            return log;
        }

        std::string FetchProgramInfoLog(GLuint program)
        {
            GLint length = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
            if (length <= 0)
                return {};
            std::string log(static_cast<size_t>(length), '\0');
            glGetProgramInfoLog(program, length, nullptr, log.data());
            if (!log.empty() && log.back() == '\0')
                log.pop_back();
            return log;
        }

        const char *GLErrorString(GLenum err)
        {
            switch (err)
            {
            case GL_NO_ERROR:
                return "GL_NO_ERROR";
            case GL_INVALID_ENUM:
                return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:
                return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:
                return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY:
                return "GL_OUT_OF_MEMORY";
            default:
                return "GL_UNKNOWN";
            }
        }
    }

    bool GLObjectLog::CheckShaderCompile(GLuint shader, std::string_view tag)
    {
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (success)
            return true;

        const std::string log = FetchShaderInfoLog(shader);
        if (tag.empty())
            spdlog::error("셰이더 컴파일 실패: {}", log);
        else
            spdlog::error("셰이더 컴파일 실패 [{}]: {}", tag, log);
        return false;
    }

    bool GLObjectLog::CheckProgramLink(GLuint program, std::string_view tag)
    {
        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (success)
            return true;

        const std::string log = FetchProgramInfoLog(program);
        if (tag.empty())
            spdlog::error("프로그램 링크 실패: {}", log);
        else
            spdlog::error("프로그램 링크 실패 [{}]: {}", tag, log);
        return false;
    }

    bool GLObjectLog::CheckProgramValidate(GLuint program, std::string_view tag)
    {
        glValidateProgram(program);
        GLint success = 0;
        glGetProgramiv(program, GL_VALIDATE_STATUS, &success);
        if (success)
            return true;

        const std::string log = FetchProgramInfoLog(program);
        if (tag.empty())
            spdlog::warn("프로그램 검증 실패: {}", log);
        else
            spdlog::warn("프로그램 검증 실패 [{}]: {}", tag, log);
        return false;
    }

    bool GLObjectLog::CheckExpectedUniforms(
        GLuint program,
        std::initializer_list<const char *> names,
        std::string_view tag)
    {
        const auto it = detail::uniformChecked.find(program);
        if (it != detail::uniformChecked.end())
            return it->second; // 캐시 히트 — 로그 없이 반환

        std::vector<const char *> missing;
        for (const char *name : names)
        {
            if (glGetUniformLocation(program, name) < 0)
                missing.push_back(name);
        }

        const bool allFound = missing.empty();
        if (!allFound)
        {
            if (tag.empty())
                spdlog::warn("프로그램 {}에 기대 uniform 누락: [{}]", program, fmt::join(missing, ", "));
            else
                spdlog::warn("프로그램 {} [{}]에 기대 uniform 누락: [{}]",
                             program, tag, fmt::join(missing, ", "));
        }

        detail::uniformChecked[program] = allFound;
        return allFound;
    }

    bool GLObjectLog::CheckExpectedAttributes(
        GLuint program,
        std::initializer_list<const char *> names,
        std::string_view tag)
    {
        const auto it = detail::attribChecked.find(program);
        if (it != detail::attribChecked.end())
            return it->second;

        std::vector<const char *> missing;
        for (const char *name : names)
        {
            if (glGetAttribLocation(program, name) < 0)
                missing.push_back(name);
        }

        const bool allFound = missing.empty();
        if (!allFound)
        {
            if (tag.empty())
                spdlog::warn("프로그램 {}에 기대 attribute 누락: [{}]", program, fmt::join(missing, ", "));
            else
                spdlog::warn("프로그램 {} [{}]에 기대 attribute 누락: [{}]",
                             program, tag, fmt::join(missing, ", "));
        }

        detail::attribChecked[program] = allFound;
        return allFound;
    }

    void GLObjectLog::InvalidateProgramCache(GLuint program)
    {
        detail::uniformChecked.erase(program);
        detail::attribChecked.erase(program);
    }

    bool GLDebug::CheckGLGenVertexArrays()
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_VALUE:
            spdlog::error("glGenVertexArrays: n<0 (전달값 1)");
            return false;
        default:
            spdlog::error("glGenVertexArrays: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }
    bool GLDebug::CheckGLBindVertexArray(const GLuint vao)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_OPERATION:
            spdlog::error("glBindVertexArray: 유효하지 않은 VAO 핸들 ({}) — "
                          "glGenVertexArrays 미반환 또는 이미 삭제됨",
                          vao);
            return false;
        default:
            spdlog::error("glBindVertexArray: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }

    bool GLDebug::CheckGLGenBuffers(const GLuint vbo)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_ENUM:
            spdlog::error("glBindBuffer: 허용되지 않는 target enum "
                          "(GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, ... 이어야 함)");
            return false;
        case GL_INVALID_VALUE:
            spdlog::error("glBindBuffer: 버퍼 ({})가 glGenBuffers 미반환 핸들", vbo);
            return false;
        default:
            spdlog::error("glBindBuffer: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }

    bool GLDebug::CheckGLBindBuffer(const GLuint vbo)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_ENUM:
            spdlog::error("glBindBuffer: 허용되지 않는 target enum "
                          "(GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER, ... 이어야 함)");
            return false;
        case GL_INVALID_VALUE:
            spdlog::error("glBindBuffer: 버퍼 ({})가 glGenBuffers 미반환 핸들", vbo);
            return false;
        default:
            spdlog::error("glBindBuffer: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }

    bool GLDebug::CheckGLBufferData(const size_t data_size)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_ENUM:
            spdlog::error("glBufferData: 허용되지 않는 target 또는 usage");
            return false;
        case GL_INVALID_VALUE:
            spdlog::error("glBufferData: size<0 ({})", data_size);
            return false;
        case GL_INVALID_OPERATION:
            spdlog::error("glBufferData: 버퍼 0 바인딩됨 (= glBindBuffer 미호출), "
                          "또는 버퍼가 현재 mapped 상태");
            return false;
        case GL_OUT_OF_MEMORY:
            spdlog::error("glBufferData: GPU 메모리 부족 ({} bytes)", data_size);
            return false;
        default:
            spdlog::error("glBufferData: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }

    bool GLDebug::CheckGLEnableVertexAttribArray(GLuint layout_location)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_OPERATION:
            spdlog::error("glEnableVertexAttribArray: VAO 미바인딩 (3.3 core 강제)");
            return false;
        case GL_INVALID_VALUE:
            spdlog::error("glEnableVertexAttribArray: index >= GL_MAX_VERTEX_ATTRIBS");
            return false;
        default:
            spdlog::error("glEnableVertexAttribArray: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }
    bool GLDebug::CheckGLVertexAttribPointer(const std::vector<GLsizei> &&strides)
    {
        const GLenum err = glGetError();
        switch (err)
        {
        case GL_NO_ERROR:
            return true;
        case GL_INVALID_VALUE:
            spdlog::error("glVertexAttribPointer: index>=max, size가 {{1,2,3,4,GL_BGRA}} 아님, "
                          "또는 stride<0 ({})",
                          fmt::join(strides, ","));
            return false;
        case GL_INVALID_ENUM:
            spdlog::error("glVertexAttribPointer: 허용되지 않는 type "
                          "(GL_FLOAT, GL_INT, GL_HALF_FLOAT, ... 이어야 함)");
            return false;
        case GL_INVALID_OPERATION:
            spdlog::error("glVertexAttribPointer: VAO 미바인딩, 또는 "
                          "non-zero offset인데 VBO 미바인딩");
            return false;
        default:
            spdlog::error("glVertexAttribPointer: 예기치 않은 오류 0x{:x}", err);
            return false;
        }
    }
}
