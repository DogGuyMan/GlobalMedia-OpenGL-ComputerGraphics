/**
 * @file vertex_layout.h
 * @brief VAO RAII 래퍼 — `glGenVertexArrays` / `glVertexAttribPointer` 자원 + 레이아웃 묶음.
 *
 * @details
 *  ### 책임
 *  - **VAO 생성/소멸** — `glGenVertexArrays` / `glDeleteVertexArrays` 의 RAII 관리.
 *  - **Vertex Attribute 레이아웃 설정** — `glVertexAttribPointer` + `glEnableVertexAttribArray`
 *    한 묶음을 진단 통합 setter (`TrySetAttrib`) 로 캡슐화.
 *  - **Bind** — 현재 컨텍스트에 VAO 활성화. 후속 VBO 바인딩/draw 호출이 본 VAO 의 상태를 사용.
 *
 *  ### 다른 GL 객체와의 경계
 *  - **VAO** = 정점 데이터의 *구조* descriptor (본 클래스).
 *    예: Position vec3, Color RGB(vec3)/RGBA(vec4), UV vec2 — stride/offset 등.
 *  - **VBO/EBO** = 정점/인덱스 *데이터*. `SJH::Buffer` (`src/buffer/`) 가 담당. VAO 가 *어떤* VBO 를
 *    참조하는지 기억하므로, `Buffer::Bind()` 와 `VertexLayout::Bind()` 의 *호출 순서* 가 중요.
 *
 *  ### 사용 흐름 (3.3 core 강제 순서)
 *  ```
 *  VAO 생성/바인딩  ->  VBO 생성/바인딩 + 데이터 업로드  ->  TrySetAttrib (layout 기록)
 *  ```
 *  본 클래스의 `Create` 는 생성 직후 자동 바인딩. 호출자가 이어서 VBO + `TrySetAttrib` 호출.
 *
 *  ### 컨벤션
 *  - 팩토리 패턴 + RAII 동기는 [.claude/architecture.md §3](../../.claude/architecture.md) 참조.
 *  - `TrySetAttrib` 는 fallible — 내부 진단(`GLDebug::CheckGLEnableVertexAttribArray` /
 *    `CheckGLVertexAttribPointer`)이 false 반환 시 즉시 `false` 전파.
 */

#ifndef __VERTEX_LAYOUT_H__
#define __VERTEX_LAYOUT_H__

#include "common/common.h"
#include "GL/gl3w.h"

namespace SJH
{
    CLASS_PTR(VertexLayout)

    class VertexLayout
    {
    public:
        /// @brief VAO 1개 생성 + 자동 바인딩.
        /// @return 항상 유효한 @c VertexLayoutUPtr (현재 `glGenVertexArrays` 실패 케이스 미처리 — 사실상 항상 성공).
        /// @details 생성 직후 `Bind()` 까지 호출되어 *현재 컨텍스트의 활성 VAO* 가 됨.
        ///          이어서 호출자가 VBO/EBO 바인딩 + `TrySetAttrib` 호출하는 흐름.
        static VertexLayoutUPtr Create();

        /// @brief @c glDeleteVertexArrays 호출 (핸들이 0 이 아닐 때만).
        ~VertexLayout();

        /// @brief 내부 VAO 핸들 반환 — 디버깅 / 직접 GL 호출 시 사용.
        GLuint GetVAOAddr() const { return mVertexArrayObject; }

        /// @brief 본 VAO 를 현재 컨텍스트에 바인딩.
        /// @return `glBindVertexArray` 후 진단 통과 시 @c true. 실패 시 spdlog 출력 + @c false.
        /// @note 같은 VAO 를 여러 번 바인딩해도 문제 없음 (GL 의 멱등 동작).
        bool Bind() const;

        /// @brief Vertex Attribute 한 슬롯 활성화 + layout 설정.
        /// @param attrib_index 셰이더의 `layout(location=N) in ...` 의 N 과 일치해야 함.
        /// @param count        구성 요소 개수 (1=scalar, 2=vec2, 3=vec3, 4=vec4 또는 GL_BGRA).
        /// @param type         원소 타입 (@c GL_FLOAT / @c GL_INT / @c GL_HALF_FLOAT 등).
        /// @param normalized   integer 타입을 [0..1] 또는 [-1..1] 로 정규화할지.
        /// @param stride       다음 정점까지의 바이트 거리 (interleaved 시 정점 전체 크기).
        /// @param offset       VBO 시작점부터 본 attribute 의 byte 오프셋.
        /// @return 두 진단 (`CheckGLEnableVertexAttribArray` + `CheckGLVertexAttribPointer`) 모두 통과 시 @c true.
        /// @note 호출 전 *VAO + 대상 VBO 가 바인딩되어 있어야 함*. 미바인딩 시
        ///       `GL_INVALID_OPERATION` (3.3 core 강제) 으로 실패.
        bool TrySetAttrib(GLuint attrib_index, int count, GLuint type,
                          bool normalized, GLsizei stride, uint64_t offset);

        /// @brief Vertex Attribute 슬롯 비활성화 (`glDisableVertexAttribArray`).
        /// @note 현재 미구현 — 향후 동적 attribute 변경 시나리오에서 필요.
        void DisableAttrib(int attrib_idx) const;

    private:
        VertexLayout() = default;

        /// @brief @c glGenVertexArrays + 즉시 @c Bind. `Create()` 내부에서만 호출.
        void Init();

        GLuint mVertexArrayObject{0};
    };
}

#endif // __VERTEX_LAYOUT_H__
