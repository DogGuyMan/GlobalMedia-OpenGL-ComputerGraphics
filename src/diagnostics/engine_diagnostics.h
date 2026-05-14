/**
 * @file engine_diagnostics.h
 * @brief chapter7 형(型) Engine 레이어 진단 — 메쉬 / 텍스처 / 라이팅 셰이더 / Transform 계층.
 *
 * @details
 *  ### 책임
 *  - `CheckInterleavedVertexBuffer` — VBO 에 올리기 전 interleaved float 배열 검증.
 *    stride 배수, position.w(점/방향), normal 의 NaN/0(퇴화 면)/단위길이.
 *  - `CheckTextureFormat` — `stbi_load` 가 보고한 채널 수와 GL `format` 의 정합성 (JPG↔PNG 흔한 버그).
 *  - `CheckExpectedLightingUniforms` — chapter7 라이팅/MVP 셰이더가 약속한 uniform 집합이 살아있는지
 *    한 번에 검증 (`GLObjectLog::CheckExpectedUniforms` 에 위임, program 별 1회 캐시 재사용).
 *  - `CheckNoParentCycle` — 부모 체인을 따라가다 사이클/과도한 깊이가 있는지 (Transform::GetModelMatrix
 *    무한재귀 → 스택오버플로 가드). 콜백으로 부모를 얻으므로 Engine 헤더에 의존하지 않음.
 *
 *  ### 비-책임
 *  - ❌ GL 호출 직후 glGetError — `gl_log.h` 의 @c GLDebug.
 *  - ❌ 셰이더 컴파일/링크/검증 — `gl_log.h` 의 @c GLObjectLog.
 *  - ❌ 누락 uniform warn-once / 타입 불일치 — `uniform_diagnostics.h`.
 *  - ❌ 현재 GL 상태 한 줄 덤프 — `gl_state_log.h`.
 *  - ❌ dangling parent(RemoveModel 후) — 호출자가 유효성을 알 수 없어 범위 밖. 사이클만 다룸.
 *
 *  ### 호출 시점 (권장)
 *  - `CheckInterleavedVertexBuffer` : `glBufferData` 직전 (= ModelBase::Build).
 *  - `CheckTextureFormat`           : `stbi_load` 직후, `glTexImage2D` 직전.
 *  - `CheckExpectedLightingUniforms`: program 링크 직후 한 번 (1회 캐시라 매 프레임 호출해도 부담 0).
 *  - `CheckNoParentCycle`           : Transform 계층에 부모를 붙이는 시점 (= ShaderProgram::AddModel).
 */

#ifndef __SJH_DIAGNOSTICS_ENGINE_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_ENGINE_DIAGNOSTICS_H__

#include <GL/gl3w.h>

#include <cstddef>
#include <string_view>
#include <vector>

namespace SJH::Diagnostics
{
    class EngineDiagnostics
    {
    public:
        EngineDiagnostics()                                     = delete;
        EngineDiagnostics(const EngineDiagnostics &)            = delete;
        EngineDiagnostics &operator=(const EngineDiagnostics &) = delete;

        /// @brief interleaved 정점 배열 검증 — VBO 업로드 직전 호출.
        /// @param data         업로드할 float 배열 ([px py pz pw  r g b a  nx ny nz  s t] 같은 형식).
        /// @param vertexLen    정점 1개당 float 개수 (= stride / sizeof(float)). 예: chapter7 의 13.
        /// @param posOffset    정점 내 position 시작 float 인덱스 (보통 0).
        /// @param posSize      position 성분 수 (4 면 .w 까지 검사, 3 이면 .w 검사 skip).
        /// @param normalOffset 정점 내 normal 시작 float 인덱스.
        /// @param normalSize   normal 성분 수 (>=3 이어야 길이/NaN 검사 수행).
        /// @param tag          로그 식별자(예: 메쉬 이름). 비우면 통과 시 info 출력 생략.
        /// @return 치명적 구조 오류(빈 배열 / stride 불일치 / 범위 부족) 없으면 @c true.
        ///         normal NaN·퇴화·비단위, position.w 이상은 warn 만 하고 @c true 유지(데이터 자체는 업로드 가능).
        static bool CheckInterleavedVertexBuffer(const std::vector<GLfloat> &data,
                                                 int vertexLen,
                                                 int posOffset, int posSize,
                                                 int normalOffset, int normalSize,
                                                 std::string_view tag = {});

        /// @brief stbi 채널 수 ↔ GL format 정합성 검사 — `glTexImage2D` 직전 호출.
        /// @param path           로그용 이미지 경로.
        /// @param nrChannels     stbi_load 가 채워준 채널 수 (1~4). 0/음수면 stbi 실패 추정.
        /// @param width,height   stbi 가 보고한 픽셀 크기. <=0 이면 로드 실패로 보고.
        /// @param format         glTexImage2D 의 @c format (GL_RED/GL_RG/GL_RGB/GL_RGBA/GL_BGR/GL_BGRA).
        /// @param internalFormat glTexImage2D 의 internalformat (참고용 로그에만 사용).
        /// @param tag            로그 식별자. 비우면 path 를 그대로 식별자로 씀.
        /// @return 크기 유효 + 채널/포맷 일치 시 @c true. 불일치는 warn 후 @c false (특히 기대 채널 > 실제 →
        ///         GPU 가 범위 밖 메모리 읽기).
        static bool CheckTextureFormat(std::string_view path,
                                       int nrChannels, int width, int height,
                                       GLenum format, GLint internalFormat,
                                       std::string_view tag = {});

        /// @brief chapter7 라이팅/MVP 셰이더가 약속한 uniform 들이 모두 존재하는지 한 번에 검증.
        /// @details 검사 대상(고정 계약): inModelMat/inViewMat/inProjMat + inLightPos/inLightColor/
        ///          inViewPos/inAmbientStrength/inSpecularStrength/inShininess/inLightingEnabled.
        ///          @c GLObjectLog::CheckExpectedUniforms 에 위임 — program 별 1회만 실검사 후 캐시.
        /// @return 모두 존재하면 @c true. (옵티마이저가 inactive uniform 을 지우는 건 정상이므로 warn 레벨.)
        static bool CheckExpectedLightingUniforms(GLuint program, std::string_view tag = {});

        /// @brief 부모 체인 사이클 / 과도한 깊이 검사 — Transform 부모 연결 시점에 호출.
        /// @tparam Node       노드 타입 (예: Transform).
        /// @tparam ParentFn   `const Node* (const Node*)` 형태 호출 가능 객체 — 부모(없으면 nullptr) 반환.
        /// @param start       체인을 거슬러 올라가기 시작할 노드.
        /// @param getParent   부모 조회 콜백.
        /// @param tag         로그 식별자(예: 모델 이름).
        /// @return 사이클 없고 깊이가 한계(@c kMaxDepth=1024) 이내면 @c true. 아니면 warn 후 @c false.
        /// @note 헤더 전용 — Node 내부 필드를 모르므로 로그엔 포인터/깊이만 출력.
        template <typename Node, typename ParentFn>
        static bool CheckNoParentCycle(const Node *start, ParentFn getParent,
                                       std::string_view tag = {});

    private:
        /// @brief CheckNoParentCycle 의 (Node 타입 무관) 보고 본체 — cpp 에 정의해 stderr 출력 일원화.
        static void ReportParentCycle(const void *start, std::size_t depth,
                                      bool depthExceeded, std::string_view tag);
    };

    // ===== 헤더 전용 템플릿 구현 =====

    template <typename Node, typename ParentFn>
    bool EngineDiagnostics::CheckNoParentCycle(const Node *start, ParentFn getParent,
                                               std::string_view tag)
    {
        constexpr std::size_t kMaxDepth = 1024;
        if (start == nullptr)
            return true;

        // Floyd 토끼-거북이: slow 1칸 / fast 2칸. 사이클이면 반드시 만남.
        const Node *slow = start;
        const Node *fast = start;
        std::size_t steps = 0;
        while (fast != nullptr)
        {
            fast = getParent(fast);
            if (fast == nullptr)
                break;
            fast = getParent(fast);
            slow = getParent(slow);
            if (slow == fast && slow != nullptr)
            {
                ReportParentCycle(static_cast<const void *>(start), steps, false, tag);
                return false;
            }
            if (++steps > kMaxDepth)
            {
                ReportParentCycle(static_cast<const void *>(start), steps, true, tag);
                return false;
            }
        }
        return true;
    }
}

#endif // __SJH_DIAGNOSTICS_ENGINE_DIAGNOSTICS_H__
