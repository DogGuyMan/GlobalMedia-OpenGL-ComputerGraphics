/**
 * @file gl_validate.cpp
 * @brief @c GLValidate 6 카테고리 구현.
 *
 * @details
 *  doc/inst.md §6 구현 순서: A -> E -> F -> B -> D -> C (단순 -> 복잡).
 *  본 파일은 같은 순서로 함수 배치.
 */

#include "diagnostics/gl_validate.h"

#include <spdlog/spdlog.h>
#include <algorithm>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace SJH::Diagnostics::GLValidate
{
    namespace
    {
        /// Cat E rate-limit 캐시 — (error_code, tag) 쌍이 이미 보고됐는지.
        /// thread_local 아니라 *static* — 단일 GL thread 가정 (GL 3.3 core).
        std::unordered_set<std::string> sReportedErrors;

        const char* GLErrorName(GLenum err)
        {
            switch (err)
            {
            case GL_NO_ERROR:                      return "GL_NO_ERROR";
            case GL_INVALID_ENUM:                  return "GL_INVALID_ENUM";
            case GL_INVALID_VALUE:                 return "GL_INVALID_VALUE";
            case GL_INVALID_OPERATION:             return "GL_INVALID_OPERATION";
            case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
            case GL_OUT_OF_MEMORY:                 return "GL_OUT_OF_MEMORY";
            default:                               return "GL_UNKNOWN";
            }
        }

        /// GLSL 타입 -> vec 컴포넌트 수 (Cat B 비교용)
        int GLTypeComponents(GLenum type)
        {
            switch (type)
            {
            case GL_FLOAT:                                  return 1;
            case GL_FLOAT_VEC2: case GL_INT_VEC2:           return 2;
            case GL_FLOAT_VEC3: case GL_INT_VEC3:           return 3;
            case GL_FLOAT_VEC4: case GL_INT_VEC4:           return 4;
            case GL_FLOAT_MAT2:                             return 4;   // 2x2
            case GL_FLOAT_MAT3:                             return 9;   // 3x3
            case GL_FLOAT_MAT4:                             return 16;  // 4x4
            case GL_INT: case GL_UNSIGNED_INT:              return 1;
            default:                                        return 0;   // unknown
            }
        }

        bool IsSamplerType(GLenum t)
        {
            return t == GL_SAMPLER_2D || t == GL_SAMPLER_CUBE
                || t == GL_SAMPLER_2D_ARRAY || t == GL_SAMPLER_3D;
        }

        /// active uniform / attribute 이름 max length 사전 조회 후 충분히 큰 buf로 query.
        int QueryMaxNameLen(GLuint program, GLenum which /*GL_ACTIVE_UNIFORM_MAX_LENGTH 등*/)
        {
            GLint maxLen = 0;
            glGetProgramiv(program, which, &maxLen);
            return maxLen > 0 ? maxLen : 64; // fallback
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat A — CheckIndices (CPU only)
    // ──────────────────────────────────────────────────────────────────────
    size_t CheckIndices(const std::vector<uint32_t>& indices, size_t vertexCount,
                        const char* tag)
    {
        size_t violations = 0;

        if (indices.empty())
        {
            spdlog::warn("[GLValidate/{}/Cat A] indices empty", tag);
            return 1;
        }
        if (indices.size() % 3 != 0)
        {
            spdlog::warn("[GLValidate/{}/Cat A] indices.size() {} not multiple of 3 (assumes triangles)",
                         tag, indices.size());
            ++violations;
        }

        // OOB + degenerate
        std::set<std::tuple<uint32_t, uint32_t, uint32_t>> seenTriangles;
        const size_t triCount = indices.size() / 3;
        for (size_t t = 0; t < triCount; ++t)
        {
            const uint32_t a = indices[3 * t + 0];
            const uint32_t b = indices[3 * t + 1];
            const uint32_t c = indices[3 * t + 2];

            // OOB
            if (a >= vertexCount || b >= vertexCount || c >= vertexCount)
            {
                spdlog::warn("[GLValidate/{}/Cat A] triangle {} OOB: ({}, {}, {}) vs vertexCount={}",
                             tag, t, a, b, c, vertexCount);
                ++violations;
                continue;
            }

            // Degenerate (zero-area)
            if (a == b || b == c || a == c)
            {
                spdlog::warn("[GLValidate/{}/Cat A] triangle {} degenerate: ({}, {}, {})",
                             tag, t, a, b, c);
                ++violations;
                continue;
            }

            // Duplicate (sorted)
            uint32_t s0 = a, s1 = b, s2 = c;
            if (s0 > s1) std::swap(s0, s1);
            if (s1 > s2) std::swap(s1, s2);
            if (s0 > s1) std::swap(s0, s1);
            auto key = std::make_tuple(s0, s1, s2);
            if (!seenTriangles.insert(key).second)
            {
                spdlog::warn("[GLValidate/{}/Cat A] triangle {} duplicate: ({}, {}, {})",
                             tag, t, a, b, c);
                ++violations;
            }
        }
        return violations;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat E — CaptureGLError (rate-limited)
    // ──────────────────────────────────────────────────────────────────────
    bool CaptureGLError(const char* tag)
    {
        const GLenum err = glGetError();
        if (err == GL_NO_ERROR)
            return true;

        // rate-limit key = "0xCODE@tag"
        std::string key = std::string(GLErrorName(err)) + "@" + (tag ? tag : "");
        if (sReportedErrors.insert(key).second)
        {
            spdlog::warn("[GLValidate/{}/Cat E] {} (0x{:X}) — first occurrence",
                         tag ? tag : "", GLErrorName(err), err);
        }
        // 큐에 다른 에러도 있을 수 있음 — drain (rate-limit 적용)
        while (true) {
            const GLenum next = glGetError();
            if (next == GL_NO_ERROR) break;
            std::string nextKey = std::string(GLErrorName(next)) + "@" + (tag ? tag : "");
            if (sReportedErrors.insert(nextKey).second)
            {
                spdlog::warn("[GLValidate/{}/Cat E] {} (0x{:X}) — additional",
                             tag ? tag : "", GLErrorName(next), next);
            }
        }
        return false;
    }

    void ResetRateLimitCache() { sReportedErrors.clear(); }

    // ──────────────────────────────────────────────────────────────────────
    // Cat F — DumpShaderInfoLogs
    // ──────────────────────────────────────────────────────────────────────
    void DumpShaderInfoLogs(GLuint program, const char* tag)
    {
        if (program == 0) return;

        // Attached shaders
        GLint shaderCount = 0;
        glGetProgramiv(program, GL_ATTACHED_SHADERS, &shaderCount);
        std::vector<GLuint> shaders(static_cast<size_t>(shaderCount));
        if (shaderCount > 0)
        {
            glGetAttachedShaders(program, shaderCount, nullptr, shaders.data());
        }

        auto containsBad = [](std::string_view s) {
            // "error", "warning" 키워드 (대소문자 무시)
            auto lower = std::string(s);
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            return lower.find("error") != std::string::npos
                || lower.find("warning") != std::string::npos;
        };

        for (GLuint sh : shaders)
        {
            GLint logLen = 0;
            glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &logLen);
            if (logLen <= 1) continue;  // empty (1 = trailing null)
            std::string log(static_cast<size_t>(logLen), '\0');
            glGetShaderInfoLog(sh, logLen, nullptr, log.data());
            if (containsBad(log))
                spdlog::warn("[GLValidate/{}/Cat F] shader {} info log:\n{}", tag, sh, log);
            else
                spdlog::info("[GLValidate/{}/Cat F] shader {} info log:\n{}", tag, sh, log);
        }

        // Program info log
        GLint plogLen = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &plogLen);
        if (plogLen > 1)
        {
            std::string log(static_cast<size_t>(plogLen), '\0');
            glGetProgramInfoLog(program, plogLen, nullptr, log.data());
            if (containsBad(log))
                spdlog::warn("[GLValidate/{}/Cat F] program {} info log:\n{}", tag, program, log);
            else
                spdlog::info("[GLValidate/{}/Cat F] program {} info log:\n{}", tag, program, log);
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat B — CheckAttribLayout (current VAO ↔ program active attributes)
    // ──────────────────────────────────────────────────────────────────────
    size_t CheckAttribLayout(GLuint program, const char* tag)
    {
        size_t violations = 0;

        GLint attrCount = 0;
        glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &attrCount);
        const GLint maxNameLen = QueryMaxNameLen(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH);
        std::vector<char> nameBuf(static_cast<size_t>(maxNameLen + 1), '\0');

        std::unordered_set<GLint> usedLocations;

        for (GLint i = 0; i < attrCount; ++i)
        {
            GLsizei nameLen = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveAttrib(program, static_cast<GLuint>(i), maxNameLen,
                              &nameLen, &size, &type, nameBuf.data());
            // built-in attributes (gl_*)은 location -1, skip
            const GLint loc = glGetAttribLocation(program, nameBuf.data());
            if (loc < 0) continue;
            usedLocations.insert(loc);

            // VAO enabled 검사
            GLint enabled = 0;
            glGetVertexAttribiv(static_cast<GLuint>(loc), GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            if (enabled != GL_TRUE)
            {
                spdlog::warn("[GLValidate/{}/Cat B] VS uses '{}' at loc {} but VAO has it disabled",
                             tag, nameBuf.data(), loc);
                ++violations;
                continue;
            }

            // 컴포넌트 수 일치
            GLint vaoSize = 0;
            glGetVertexAttribiv(static_cast<GLuint>(loc), GL_VERTEX_ATTRIB_ARRAY_SIZE, &vaoSize);
            const int vsComp = GLTypeComponents(type);
            if (vsComp > 0 && vsComp <= 4 && vaoSize != vsComp)
            {
                spdlog::warn("[GLValidate/{}/Cat B] loc {} '{}': VS expects vec{}, VAO has size={}",
                             tag, loc, nameBuf.data(), vsComp, vaoSize);
                ++violations;
            }
        }

        // 추가: VAO가 enable 했지만 VS가 안 쓰는 location -> warning (info 레벨 — 의도일 수도)
        for (GLint loc = 0; loc < 16; ++loc)
        {
            if (usedLocations.count(loc)) continue;
            GLint enabled = 0;
            glGetVertexAttribiv(static_cast<GLuint>(loc), GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
            if (enabled == GL_TRUE)
            {
                spdlog::info("[GLValidate/{}/Cat B] VAO enabled loc {} but VS doesn't use it",
                             tag, loc);
                // violation 카운트 안 함 — 정보만
            }
        }
        return violations;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat D — CheckSamplerBindings
    // ──────────────────────────────────────────────────────────────────────
    size_t CheckSamplerBindings(GLuint program, const char* tag)
    {
        size_t violations = 0;

        GLint uniformCount = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
        const GLint maxNameLen = QueryMaxNameLen(program, GL_ACTIVE_UNIFORM_MAX_LENGTH);
        std::vector<char> nameBuf(static_cast<size_t>(maxNameLen + 1), '\0');

        GLint savedActive = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActive);

        for (GLint i = 0; i < uniformCount; ++i)
        {
            GLsizei nameLen = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(program, static_cast<GLuint>(i), maxNameLen,
                               &nameLen, &size, &type, nameBuf.data());
            if (!IsSamplerType(type)) continue;

            const GLint loc = glGetUniformLocation(program, nameBuf.data());
            if (loc < 0) continue;

            GLint unit = 0;
            glGetUniformiv(program, loc, &unit);
            if (unit < 0 || unit >= 16)
            {
                spdlog::warn("[GLValidate/{}/Cat D] sampler '{}' -> out-of-range unit {}",
                             tag, nameBuf.data(), unit);
                ++violations;
                continue;
            }

            glActiveTexture(GL_TEXTURE0 + unit);
            GLint texId = 0;
            // sampler type 에 따라 다른 binding query — 일단 GL_TEXTURE_BINDING_2D 가 가장 흔함
            const GLenum bindingEnum =
                (type == GL_SAMPLER_CUBE)    ? GL_TEXTURE_BINDING_CUBE_MAP :
                (type == GL_SAMPLER_3D)      ? GL_TEXTURE_BINDING_3D       :
                                               GL_TEXTURE_BINDING_2D;
            glGetIntegerv(bindingEnum, &texId);
            if (texId == 0)
            {
                spdlog::warn("[GLValidate/{}/Cat D] sampler '{}' -> unit {} but no texture bound",
                             tag, nameBuf.data(), unit);
                ++violations;
            }
        }

        glActiveTexture(static_cast<GLenum>(savedActive));
        return violations;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat C — CheckUniformCoverage (declared but value default-0 의심)
    // ──────────────────────────────────────────────────────────────────────
    size_t CheckUniformCoverage(GLuint program, const char* tag)
    {
        size_t violations = 0;

        GLint uniformCount = 0;
        glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniformCount);
        const GLint maxNameLen = QueryMaxNameLen(program, GL_ACTIVE_UNIFORM_MAX_LENGTH);
        std::vector<char> nameBuf(static_cast<size_t>(maxNameLen + 1), '\0');

        for (GLint i = 0; i < uniformCount; ++i)
        {
            GLsizei nameLen = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(program, static_cast<GLuint>(i), maxNameLen,
                               &nameLen, &size, &type, nameBuf.data());

            const GLint loc = glGetUniformLocation(program, nameBuf.data());
            if (loc < 0) continue;

            // sampler — unit 0이 의도된 default일 수 있으므로 Cat D에서 별도 처리
            if (IsSamplerType(type)) continue;

            // type별 값 query -> 모두 0이면 "값 미설정 의심"
            bool isZero = false;
            if (type == GL_FLOAT)
            {
                GLfloat v = 0; glGetUniformfv(program, loc, &v);
                isZero = (v == 0.0f);
            }
            else if (type == GL_FLOAT_VEC2)
            {
                GLfloat v[2] = {}; glGetUniformfv(program, loc, v);
                isZero = (v[0] == 0 && v[1] == 0);
            }
            else if (type == GL_FLOAT_VEC3)
            {
                GLfloat v[3] = {}; glGetUniformfv(program, loc, v);
                isZero = (v[0] == 0 && v[1] == 0 && v[2] == 0);
            }
            else if (type == GL_FLOAT_VEC4)
            {
                GLfloat v[4] = {}; glGetUniformfv(program, loc, v);
                isZero = (v[0] == 0 && v[1] == 0 && v[2] == 0 && v[3] == 0);
            }
            else if (type == GL_FLOAT_MAT4)
            {
                GLfloat v[16] = {}; glGetUniformfv(program, loc, v);
                isZero = std::all_of(std::begin(v), std::end(v),
                                     [](float x){ return x == 0.0f; });
            }
            else if (type == GL_INT)
            {
                GLint v = 0; glGetUniformiv(program, loc, &v);
                isZero = (v == 0);
            }
            // 다른 타입 — 보수적으로 skip

            if (isZero)
            {
                spdlog::warn("[GLValidate/{}/Cat C] uniform '{}' value default-zero — "
                             "CPU 측 setter 호출 누락 의심?",
                             tag, nameBuf.data());
                ++violations;
            }
        }
        return violations;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Cat G — CheckViewport (GL viewport ↔ 기대 렌더 타깃 크기)
    // ──────────────────────────────────────────────────────────────────────
    size_t CheckViewport(int expectedWidth, int expectedHeight, const char* tag)
    {
        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);

        // vp = [x, y, width, height]. 크기(2,3)만 비교 — 원점 오프셋은 정상적으로 0이 아닐 수도.
        if (vp[2] != expectedWidth || vp[3] != expectedHeight)
        {
            spdlog::warn("[GLValidate/{}/Cat G] viewport [{}, {}, {}, {}] != expected {}x{} "
                         "— viewport 미설정 또는 HiDPI 논리/물리 픽셀 불일치 의심",
                         tag, vp[0], vp[1], vp[2], vp[3],
                         expectedWidth, expectedHeight);
            return 1;
        }
        spdlog::info("[GLValidate/{}/Cat G] viewport {}x{} matches expected target",
                     tag, vp[2], vp[3]);
        return 0;
    }

    // ──────────────────────────────────────────────────────────────────────
    // RunFullSweep — A + B + C + D + F (E는 별도 호출)
    // ──────────────────────────────────────────────────────────────────────
    size_t RunFullSweep(GLuint program, const std::vector<uint32_t>& indices,
                        size_t vertexCount, const char* tag)
    {
        size_t total = 0;
        total += CheckIndices(indices, vertexCount, tag);
        total += CheckAttribLayout(program, tag);
        total += CheckUniformCoverage(program, tag);
        total += CheckSamplerBindings(program, tag);
        DumpShaderInfoLogs(program, tag);

        if (total == 0)
            spdlog::info("[GLValidate/{}] all clean ({} index trios, program {})",
                         tag, indices.size() / 3, program);
        else
            spdlog::warn("[GLValidate/{}] total violations: {}", tag, total);
        return total;
    }
}
