/**
 * @file engine_diagnostics.cpp
 * @brief @c EngineDiagnostics 구현 — 메쉬 / 텍스처 / 라이팅 / Transform 사이클.
 */

#include "engine_diagnostics.h"

#include "gl_log.h"
#include "log_util.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace SJH::Diagnostics
{
    namespace
    {
        /// GL pixel-format enum → 기대 채널 수. 모르는 enum 은 0 (검사 skip 신호).
        int ExpectedChannels(GLenum format)
        {
            switch (format)
            {
            case GL_RED:
                return 1;
            case GL_RG:
                return 2;
            case GL_RGB:
            case GL_BGR:
                return 3;
            case GL_RGBA:
            case GL_BGRA:
                return 4;
            default:
                return 0;
            }
        }

        const char *FormatName(GLenum e)
        {
            switch (e)
            {
            case GL_RED:
                return "GL_RED";
            case GL_RG:
                return "GL_RG";
            case GL_RGB:
                return "GL_RGB";
            case GL_BGR:
                return "GL_BGR";
            case GL_RGBA:
                return "GL_RGBA";
            case GL_BGRA:
                return "GL_BGRA";
            default:
                return "(0x?)"; // 호출부에서 hex 도 같이 찍으므로 충분
            }
        }

        bool IsFinite(float v)
        {
            return std::isfinite(v);
        }
    }

    bool EngineDiagnostics::CheckInterleavedVertexBuffer(const std::vector<GLfloat> &data,
                                                         int vertexLen,
                                                         int posOffset, int posSize,
                                                         int normalOffset, int normalSize,
                                                         std::string_view tag)
    {
        const auto fail = [&](const std::string &msg) {
            if (tag.empty())
                Log::Warn("정점 버퍼 검증 실패: {}", msg);
            else
                Log::Warn("정점 버퍼 검증 실패 [{}]: {}", tag, msg);
            return false;
        };

        if (vertexLen <= 0)
            return fail(Log::Format("vertexLen 가 비정상: {}", vertexLen));
        if (data.empty())
            return fail("buffer_data 가 비어있음");
        if (data.size() % static_cast<std::size_t>(vertexLen) != 0)
            return fail(Log::Format("크기({})가 vertexLen({}) 배수가 아님", data.size(), vertexLen));
        if (posOffset < 0 || posSize < 0 || posOffset + posSize > vertexLen)
            return fail(Log::Format("position 범위(off={}, size={})가 vertexLen({}) 밖", posOffset, posSize, vertexLen));
        if (normalSize > 0 && (normalOffset < 0 || normalOffset + normalSize > vertexLen))
            return fail(Log::Format("normal 범위(off={}, size={})가 vertexLen({}) 밖", normalOffset, normalSize, vertexLen));

        const std::size_t vcount = data.size() / static_cast<std::size_t>(vertexLen);

        // ----- 비치명 검사들: warn 만 하고 true 유지. 너무 시끄럽지 않게 처음 몇 개만 샘플 출력. -----
        constexpr int kSample = 5;

        int badW = 0;
        std::string badWSamples;
        int badNormal = 0;
        std::string badNormalSamples;

        for (std::size_t v = 0; v < vcount; ++v)
        {
            const std::size_t base = v * static_cast<std::size_t>(vertexLen);

            if (posSize >= 4)
            {
                const float w = data[base + static_cast<std::size_t>(posOffset) + 3];
                const bool ok = IsFinite(w) && (std::fabs(w) < 1e-3f || std::fabs(w - 1.0f) < 1e-3f);
                if (!ok)
                {
                    if (badW < kSample)
                        badWSamples += Log::Format("{}v#{}=w{}", badW == 0 ? "" : ", ", v, w);
                    ++badW;
                }
            }

            if (normalSize >= 3)
            {
                const float nx = data[base + static_cast<std::size_t>(normalOffset) + 0];
                const float ny = data[base + static_cast<std::size_t>(normalOffset) + 1];
                const float nz = data[base + static_cast<std::size_t>(normalOffset) + 2];
                const char *why = nullptr;
                if (!IsFinite(nx) || !IsFinite(ny) || !IsFinite(nz))
                    why = "NaN/Inf"; // ComputeFaceNormal 이 퇴화 삼각형에서 0/0 → NaN
                else
                {
                    const float len2 = nx * nx + ny * ny + nz * nz;
                    if (len2 < 1e-12f)
                        why = "zero(퇴화 면)";
                    else if (std::fabs(std::sqrt(len2) - 1.0f) > 0.05f)
                        why = "비단위";
                }
                if (why != nullptr)
                {
                    if (badNormal < kSample)
                        badNormalSamples += Log::Format("{}v#{}({},{},{}:{})",
                                                        badNormal == 0 ? "" : ", ", v, nx, ny, nz, why);
                    ++badNormal;
                }
            }
        }

        const char *tagPart = tag.empty() ? "" : " [";
        const char *tagEnd  = tag.empty() ? "" : "]";

        if (badW > 0)
            Log::Warn("정점 버퍼{}{}{}: position.w 가 0/1 이 아닌 정점 {}개 — {}{}",
                      tagPart, tag, tagEnd, badW, badWSamples, badW > kSample ? " ..." : "");
        if (badNormal > 0)
            Log::Warn("정점 버퍼{}{}{}: 비정상 normal 정점 {}개 — {}{}",
                      tagPart, tag, tagEnd, badNormal, badNormalSamples, badNormal > kSample ? " ..." : "");

        if (badW == 0 && badNormal == 0 && !tag.empty())
            Log::Info("정점 버퍼 [{}]: OK — 정점 {}개, stride {} float", tag, vcount, vertexLen);

        return true;
    }

    bool EngineDiagnostics::CheckTextureFormat(std::string_view path,
                                               int nrChannels, int width, int height,
                                               GLenum format, GLint internalFormat,
                                               std::string_view tag)
    {
        const std::string id(tag.empty() ? path : tag);

        if (width <= 0 || height <= 0 || nrChannels <= 0)
        {
            Log::Warn("텍스처 [{}]: 로드 실패 추정 (w={}, h={}, ch={})", id, width, height, nrChannels);
            return false;
        }

        const int want = ExpectedChannels(format);
        if (want == 0)
        {
            // 모르는 format enum — 검사 불가, 정보만.
            Log::Info("텍스처 [{}]: {}x{}, ch={}, format=0x{:x}, internal=0x{:x}",
                      id, width, height, nrChannels, format, static_cast<unsigned>(internalFormat));
            return true;
        }

        if (want != nrChannels)
        {
            if (want > nrChannels)
                Log::Warn("텍스처 [{}]: format {}({}채널)가 실제 ch={} 보다 많음 — "
                          "glTexImage2D 가 픽셀당 범위 밖 메모리를 읽음! (JPG 를 GL_RGBA 로 로드한 케이스?)",
                          id, FormatName(format), want, nrChannels);
            else
                Log::Warn("텍스처 [{}]: format {}({}채널)가 실제 ch={} 보다 적음 — "
                          "여분 채널 무시됨 (PNG 의 alpha 손실 등)",
                          id, FormatName(format), want, nrChannels);
            return false;
        }

        Log::Info("텍스처 [{}]: OK — {}x{}, ch={}, format={}, internal=0x{:x}",
                  id, width, height, nrChannels, FormatName(format), static_cast<unsigned>(internalFormat));
        return true;
    }

    bool EngineDiagnostics::CheckExpectedLightingUniforms(GLuint program, std::string_view tag)
    {
        // chapter7 라이팅/MVP 셰이더의 "고정 계약" — 한 곳에 모아 둠.
        // 이름이 main.cpp 의 UNIFORM_* 상수와 갈라지면 여기서 잡힌다.
        return GLObjectLog::CheckExpectedUniforms(
            program,
            {"inModelMat", "inViewMat", "inProjMat",
             "inLightPos", "inLightColor", "inViewPos",
             "inAmbientStrength", "inSpecularStrength", "inShininess", "inLightingEnabled"},
            tag);
    }

    void EngineDiagnostics::ReportParentCycle(const void *start, std::size_t depth,
                                              bool depthExceeded, std::string_view tag)
    {
        const char *tagPart = tag.empty() ? "" : " [";
        const char *tagEnd  = tag.empty() ? "" : "]";
        const auto startAddr = reinterpret_cast<std::uintptr_t>(start);
        if (depthExceeded)
            Log::Warn("Transform 계층{}{}{}: 부모 체인 깊이가 {} 초과 — 사이클 의심 (GetModelMatrix 무한재귀 위험). start=0x{:x}",
                      tagPart, tag, tagEnd, depth, startAddr);
        else
            Log::Warn("Transform 계층{}{}{}: 부모 체인에 사이클 발견 (깊이 ~{}) — GetModelMatrix 가 스택오버플로 남. start=0x{:x}",
                      tagPart, tag, tagEnd, depth, startAddr);
    }
}
