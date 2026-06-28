/**
 * @file gl_state_fields.h
 * @brief 한 시점의 GL 상태(POD struct) + 캡처/포맷팅 자유 함수 - production / test 공유.
 *
 * @details
 *  ### 책임 (Task 1 / audit 트랙 A)
 *  - @c VertexAttribInfo - 한 vertex attribute slot 의 layout 상태 (size/type/stride/binding).
 *  - @c GLStateFields - 한 시점의 GL 바인딩 + 픽셀 파이프라인 + viewport + attribute 배열 스냅샷.
 *  - @c CaptureGLState - 부수효과 0 캡처 (active_texture 저장->유닛 순회->복원).
 *  - @c SymbolicName - GLenum -> 사람이 읽는 이름 (~28 사전 + GL_TEXTUREn 동적).
 *  - @c FieldsToString - 멀티라인 포맷팅. enum 은 SymbolicName, GLuint 핸들은 raw (비대칭 정책).
 *
 *  ### 비-책임
 *  - [X] Production 측 디버깅 한 줄 덤프 - `gl_state_log.h` (Task 5).
 *  - [X] 테스트 측 RAII Snapshot + Diff - `test/support/gl_state_snapshot.h` (Task 6).
 *  - [X] 셰이더/uniform 진단 - `uniform_diagnostics.h`.
 *
 *  ### 비대칭 포맷팅 정책 (spec 2.3)
 *  - enum 필드 (depth_func / blend_factor / cull_face_mode 등) -> @c SymbolicName 적용.
 *  - GLuint 핸들 (vao / program / buffer 등) -> raw 정수.
 *    이유: 핸들 식별자 자체에 의미가 없음, 테스트는 *어떤 객체가 바인딩됐는지* 가 아니라
 *    *어떤 enum 정책이 활성인지* 를 단언해야 회귀 가시성이 큼.
 *
 *  ### bug-coverage-audit 카테고리 매핑
 *  - 카테고리 C (vertex attribute layout 회귀): `attribute_layouts[]` 가 잡음.
 *  - 카테고리 B (binding 회귀): `vao` / `array_buffer` / `element_buffer` / `program` 이 잡음.
 *  - 카테고리 D (픽셀 파이프라인 회귀): `depth_*` / `blend_*` / `cull_*` / `color_write_mask` 가 잡음.
 *
 * @see `doc/testplan/2026-05-07-gl-state-and-test-quality-design.md` sec.2.1
 */

#ifndef __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__
#define __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__

#pragma once

#include "GL/gl3w.h"
#include <array>
#include <string>
#include <vector>

namespace SJH::Diagnostics
{
    /**
     * @brief 한 vertex attribute slot 의 layout 상태 POD.
     * @details
     *  현재 바인딩된 VAO 의 slot 당 설정. VAO=0 일 때는 모두 default 값.
     *  @c glGetVertexAttribiv 로 조회하므로 부수효과 0.
     *
     *  ### 잡는 회귀 (bug-coverage-audit 카테고리 C)
     *  - C1: stride 잘못 계산 -> @c stride 필드.
     *  - C2: vec3 attribute 에 @c size=2 설정 -> @c size 필드.
     *  - C3: wrong VBO binding -> @c buffer_binding 필드.
     *  - C4: @c glEnableVertexAttribArray 누락 -> @c enabled 필드.
     */
    struct VertexAttribInfo
    {
        bool    enabled{false};         ///< GL_VERTEX_ATTRIB_ARRAY_ENABLED
        GLint   size{4};                ///< GL_VERTEX_ATTRIB_ARRAY_SIZE (1, 2, 3, 4) - default 4
        GLenum  type{GL_FLOAT};         ///< GL_VERTEX_ATTRIB_ARRAY_TYPE - default GL_FLOAT
        bool    normalized{false};      ///< GL_VERTEX_ATTRIB_ARRAY_NORMALIZED
        GLsizei stride{0};              ///< GL_VERTEX_ATTRIB_ARRAY_STRIDE
        GLuint  buffer_binding{0};      ///< GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING (어느 VBO에서 오는지)

        bool operator==(const VertexAttribInfo& o) const noexcept
        {
            return enabled == o.enabled && size == o.size && type == o.type
                && normalized == o.normalized && stride == o.stride
                && buffer_binding == o.buffer_binding;
        }
        bool operator!=(const VertexAttribInfo& o) const noexcept { return !(*this == o); }
    };

    /**
     * @brief 한 시점의 GL 상태 스냅샷 POD - @c GLStateLog (production 덤프) 와
     *        @c gl_state_snapshot (테스트 RAII Diff) 양쪽이 공유.
     * @details
     *  ### 필드 분류
     *  - **바인딩** (@c vao / @c program / @c array_buffer / @c element_buffer / @c draw_fbo / @c read_fbo)
     *    - bug-coverage-audit 카테고리 B (binding 회귀) 검출.
     *  - **텍스처** (@c active_texture + @c texture_2d_per_unit[16])
     *    - macOS GL 3.3 spec 상한 16 유닛. @c FieldsToString 은 0 이 아닌 유닛만 출력.
     *  - **픽셀 파이프라인** (depth/blend/cull/color_write_mask/clear_color)
     *    - 카테고리 D (픽셀 파이프라인 회귀) 검출.
     *  - **attribute_layouts[16]** - 카테고리 C (vertex attribute layout 회귀) 검출.
     *
     *  ### 포맷팅 비대칭 정책 (spec 2.3)
     *  @c FieldsToString 에서 enum 필드(@c depth_func 등)는 @c SymbolicName, @c GLuint
     *  핸들(@c vao / @c program 등)은 raw 정수로 출력 - 핸들 값 자체에는 의미가 없기 때문.
     */
    struct GLStateFields
    {
        // -- 바인딩 ----------------------------------------------------------
        GLuint vao{0};            ///< @brief 현재 바인딩된 VAO 핸들.
        GLuint program{0};        ///< @brief 현재 사용 중인 프로그램 핸들.
        GLuint array_buffer{0};   ///< @brief @c GL_ARRAY_BUFFER_BINDING.
        GLuint element_buffer{0}; ///< @brief @c GL_ELEMENT_ARRAY_BUFFER_BINDING (VAO=0 이면 항상 0).
        GLuint draw_fbo{0};       ///< @brief @c GL_DRAW_FRAMEBUFFER_BINDING.
        GLuint read_fbo{0};       ///< @brief @c GL_READ_FRAMEBUFFER_BINDING.

        // -- 텍스처 ----------------------------------------------------------
        /// @brief 현재 활성 텍스처 유닛 (@c GL_ACTIVE_TEXTURE). 기본 @c GL_TEXTURE0.
        GLenum active_texture{GL_TEXTURE0};
        /// @brief 유닛별 GL_TEXTURE_BINDING_2D. macOS GL 3.3 spec 상한 16 유닛.
        std::array<GLuint, 16> texture_2d_per_unit{};

        // -- viewport --------------------------------------------------------
        /// @brief @c GL_VIEWPORT [x, y, width, height].
        std::array<GLint, 4> viewport{};

        // -- 픽셀 파이프라인 --------------------------------------------------
        bool   depth_test_enabled{false};      ///< @brief @c glIsEnabled(GL_DEPTH_TEST).
        GLenum depth_func{GL_LESS};            ///< @brief @c GL_DEPTH_FUNC (기본 @c GL_LESS).
        bool   depth_write_mask{true};         ///< @brief @c GL_DEPTH_WRITEMASK.

        bool   blend_enabled{false};           ///< @brief @c glIsEnabled(GL_BLEND).
        GLenum blend_src_rgb{GL_ONE};          ///< @brief @c GL_BLEND_SRC_RGB.
        GLenum blend_dst_rgb{GL_ZERO};         ///< @brief @c GL_BLEND_DST_RGB.

        bool   cull_face_enabled{false};       ///< @brief @c glIsEnabled(GL_CULL_FACE).
        GLenum cull_face_mode{GL_BACK};        ///< @brief @c GL_CULL_FACE_MODE (기본 @c GL_BACK).
        GLenum front_face{GL_CCW};             ///< @brief @c GL_FRONT_FACE (기본 @c GL_CCW).

        /// @brief @c GL_COLOR_WRITEMASK - [R, G, B, A] 쓰기 허용 여부.
        std::array<bool, 4>    color_write_mask{true, true, true, true};
        /// @brief @c GL_COLOR_CLEAR_VALUE - [R, G, B, A] clear 색상.
        std::array<GLfloat, 4> clear_color{0, 0, 0, 0};

        /// @brief Vertex attribute slot 별 layout 상태 - GL 3.3 spec 상한 16.
        /// @details bug-coverage-audit 카테고리 C (vertex attribute layout 회귀) 대응.
        std::array<VertexAttribInfo, 16> attribute_layouts{};
    };

    /**
     * @brief 두 @c GLStateFields 간 바뀐 필드 1건 (A3 - 순수 CPU diff).
     * @details @c DiffStates 가 채우는 원소. @c category 는 bug-coverage-audit 매핑
     *          ('B'=binding / 'C'=vertex attribute layout / 'D'=픽셀 파이프라인).
     */
    struct FieldChange
    {
        std::string field;    ///< 바뀐 필드 이름 (예: "blend_src_rgb", "attribute_layouts[2].stride").
        std::string before;   ///< 변경 전 값의 문자열 표현 (enum 은 @c SymbolicName).
        std::string after;    ///< 변경 후 값의 문자열 표현.
        char        category; ///< 'B'(binding) / 'C'(attribute layout) / 'D'(pixel pipeline).
    };

    /**
     * @brief 두 GL 상태 스냅샷을 비교해 바뀐 필드 목록을 반환 (A3 - 순수 CPU, GL 무관).
     * @details 필드 단위로 @c operator== 가 아닌 멤버별 비교를 수행해 *무엇이* 바뀌었는지
     *          식별한다. enum 필드는 @c SymbolicName 으로, 핸들/정수는 raw 로 표기
     *          (@c FieldsToString 의 비대칭 정책 답습). 동일하면 빈 벡터.
     * @param before 변경 전 상태 (예: 패스 렌더 전 캡처).
     * @param after  변경 후 상태 (예: 패스 렌더 후 캡처).
     * @return 바뀐 필드 목록 (C-4 GL 상태누수 테스트가 소비).
     */
    std::vector<FieldChange> DiffStates(const GLStateFields& before,
                                        const GLStateFields& after);

    /**
     * @brief 현재 GL 상태를 @c GLStateFields 로 캡처해 반환.
     * @details
     *  부수효과 0 - @c active_texture 를 저장 후 유닛 0~15 순회, 복원.
     *  내부에서 pre-drain(@c while glGetError) + post-check 를 수행해 캡처 도중 GL 에러 오염 방지.
     * @pre  GL context 가 현재 스레드에 활성화되어 있어야 함 (caller 책임).
     * @post 모든 필드 채워진 @c GLStateFields 반환. 캡처 도중 GL 에러 발생 시 @c spdlog::warn
     *       (값은 채워지지만 정확성 의심 신호).
     */
    GLStateFields CaptureGLState();

    /**
     * @brief @c GLenum -> 사람이 읽는 이름 문자열.
     * @details
     *  ~28 항목 정적 사전 + @c GL_TEXTURE0..GL_TEXTURE15 동적 생성.
     *  미적중 시 @c "0xXXXX" (4자리 대문자 hex) 형식.
     * @note @c SymbolicName(0) == @c "GL_ZERO" - blend factor 컨텍스트 가정.
     *       근거는 spec 2.1 / @c test_gl_state_fields.cpp "GL_ZERO 정책" 케이스 참조.
     * @warning 반환값은 @c thread_local 정적 버퍼 - caller 가 즉시 출력해야 안전.
     *          영구 보관이 필요하면 @c std::string 으로 복사.
     */
    const char* SymbolicName(GLenum e);

    /**
     * @brief @c GLStateFields -> 사람이 읽는 다중라인 문자열.
     * @details
     *  - enum 필드(@c depth_func 등): @c SymbolicName 변환.
     *  - @c GLuint 핸들(@c vao / @c program 등): raw 정수 (의도된 비대칭).
     *  - @c texture_2d_per_unit / @c attribute_layouts : 0이 아닌/enabled 인 항목만 출력 (노이즈 최소화).
     *  - @c VAO=0 이면 @c element_buffer 라인에 주석 자동 포함
     *    (@c "EBO state is per-VAO; with VAO=0, this is always 0").
     */
    std::string FieldsToString(const GLStateFields& fields);
}

#endif // __SJH_DIAGNOSTICS_GL_STATE_FIELDS_H__
