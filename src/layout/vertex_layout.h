/**
 * @file vertex_layout.h
 * @brief VAO RAII 래퍼 - @c glGenVertexArrays / @c glVertexAttribPointer 자원 + 레이아웃 묶음.
 *
 * @details
 *  ### 책임
 *  - **VAO 생성/소멸** - @c glGenVertexArrays / @c glDeleteVertexArrays 의 RAII 관리.
 *  - **Vertex Attribute 레이아웃 설정** - @c glVertexAttribPointer + @c glEnableVertexAttribArray
 *    한 묶음을 진단 통합 setter (@c TrySetAttrib) 로 캡슐화.
 *  - **Bind** - 현재 컨텍스트에 VAO 활성화. 후속 VBO 바인딩/draw 호출이 본 VAO 의 상태를 사용.
 *
 *  ### 다른 GL 객체와의 경계
 *  - **VAO** = 정점 데이터의 *구조* descriptor (본 클래스).
 *    예: Position vec3, Color RGB(vec3)/RGBA(vec4), UV vec2 - stride/offset 등.
 *  - **VBO/EBO** = 정점/인덱스 *데이터*. @c SJH::Buffer (@c src/buffer/) 가 담당.
 *    VAO 가 *어떤* VBO 를 참조하는지 기억하므로,
 *    @c Buffer::Bind() 와 @c VertexLayout::Bind() 의 *호출 순서* 가 중요.
 *
 *  ### 사용 흐름 (GL 3.3 core 강제 순서)
 *  ```
 *  VAO 생성/바인딩  ->  VBO 생성/바인딩 + 데이터 업로드  ->  TrySetAttrib (layout 기록)
 *  ```
 *  @c Create 는 생성 직후 자동 바인딩. 호출자가 이어서 VBO + @c TrySetAttrib 호출.
 *
 *  ### 비-책임
 *  - [X] VBO/EBO 데이터 보관 - @c SJH::Buffer (@c src/buffer/) 가 담당.
 *  - [X] draw 호출 (@c glDrawArrays / @c glDrawElements) - SceneRenderer / MeshPassProcessor 가 담당.
 *  - [X] 셰이더 attribute location 자동 도출 - 호출자가 @c layout(location=N) 과 일치시킬 책임.
 *
 * @note @c TrySetAttrib 는 fallible - 내부 진단 (@c GLDebug::CheckGLEnableVertexAttribArray /
 *       @c CheckGLVertexAttribPointer) 이 false 반환 시 즉시 @c false 전파.
 *       팩토리 패턴 + RAII 동기는 @c .claude/architecture.md sec.3 참조.
 */

#ifndef __VERTEX_LAYOUT_H__
#define __VERTEX_LAYOUT_H__

#include "common/common.h"
#include "GL/gl3w.h"

namespace SJH
{
    CLASS_PTR(VertexLayout)

    /**
     * @brief OpenGL VAO(Vertex Array Object) RAII 래퍼 - 정점 attribute 레이아웃 descriptor.
     * @details
     *  VAO 핸들을 소유하고, @c TrySetAttrib 로 attribute 슬롯(location/count/type/stride/offset)을
     *  한 번에 활성화/설정한다. 소멸자에서 @c glDeleteVertexArrays 자동 호출.
     *
     *  팩토리 @c Create 만 인스턴스를 생성 가능하며, 반환 시점에 이미 VAO 가 바인딩된 상태 -
     *  호출자는 즉시 VBO 바인딩 + @c TrySetAttrib 체인을 이어갈 수 있다.
     *
     *  ### GL 3.3 core 초기화 순서
     *  ```
     *  VertexLayout::Create()           // VAO 생성 + 자동 Bind
     *  buffer->Bind(GL_ARRAY_BUFFER)    // VBO 바인딩
     *  layout->TrySetAttrib(0, 3, ...)  // position slot
     *  layout->TrySetAttrib(1, 2, ...)  // uv slot
     *  ```
     */
    class VertexLayout
    {
    public:
        /**
         * @brief VAO 1개 생성 + 자동 바인딩.
         * @return 항상 유효한 @c VertexLayoutUPtr (현재 @c glGenVertexArrays 실패 케이스 미처리 - 사실상 항상 성공).
         * @details 생성 직후 @c Bind() 까지 호출되어 *현재 컨텍스트의 활성 VAO* 가 됨.
         *          이어서 호출자가 VBO/EBO 바인딩 + @c TrySetAttrib 호출하는 흐름.
         */
        static VertexLayoutUPtr Create();

        /// @brief @c glDeleteVertexArrays 호출 (핸들이 0 이 아닐 때만).
        ~VertexLayout();

        /// @brief 내부 VAO 핸들 반환 - 디버깅 / 직접 GL 호출 시 사용.
        GLuint GetVAOAddr() const { return mVertexArrayObject; }

        /// @brief VAO GL 핸들 반환 - @c DeviceContext::BindVAO 인자. @c GetVAOAddr 의 의미론적 alias.
        GLuint GetVAO() const { return mVertexArrayObject; }

        /**
         * @brief 본 VAO 를 현재 컨텍스트에 바인딩 (@c glBindVertexArray).
         * @return 바인딩 후 진단(@c GLDebug::CheckGLBindVertexArray) 통과 시 @c true.
         *         실패 시 spdlog 에러 출력 + @c false.
         * @note 같은 VAO 를 여러 번 바인딩해도 문제 없음 (GL 의 멱등 동작).
         */
        bool Bind() const;

        /**
         * @brief Vertex Attribute 한 슬롯 활성화 + 레이아웃 설정.
         * @param attrib_index  셰이더의 @c layout(location=N) @c in 의 N 과 일치해야 함.
         * @param count         구성 요소 개수 (1=scalar, 2=vec2, 3=vec3, 4=vec4 또는 @c GL_BGRA).
         * @param type          원소 타입 (@c GL_FLOAT / @c GL_INT / @c GL_HALF_FLOAT 등).
         * @param normalized    integer 타입을 [0..1] 또는 [-1..1] 로 정규화할지.
         * @param stride        다음 정점까지의 바이트 거리 (interleaved 배치 시 정점 전체 크기).
         * @param offset        VBO 시작점부터 본 attribute 의 byte 오프셋.
         * @return @c glEnableVertexAttribArray + @c glVertexAttribPointer 두 진단 모두 통과 시 @c true.
         * @note 호출 전 *VAO + 대상 VBO 가 반드시 바인딩되어 있어야 함*.
         *       미바인딩 시 @c GL_INVALID_OPERATION (GL 3.3 core) 으로 실패.
         */
        bool TrySetAttrib(GLuint attrib_index, int count, GLuint type,
                          bool normalized, GLsizei stride, uint64_t offset);

        /**
         * @brief Vertex Attribute 슬롯 비활성화 (@c glDisableVertexAttribArray).
         * @param attrib_idx  비활성화할 attribute location (셰이더 @c layout(location=N) 의 N).
         * @note 현재 미구현(빈 몸체) - 향후 동적 attribute 변경 시나리오에서 필요.
         */
        void DisableAttrib(int attrib_idx) const;

    private:
        VertexLayout() = default;

        /**
         * @brief @c glGenVertexArrays 호출 후 즉시 @c Bind. @c Create() 내부에서만 호출.
         * @details @c glGenVertexArrays 후 @c GLDebug::CheckGLGenVertexArrays 로 진단.
         *          n=1 고정이라 @c GL_INVALID_VALUE 발생 여지는 없지만, buffer.cpp 의
         *          Gen/Bind/BufferData 체크와 *대칭성* 유지를 위해 호출.
         */
        void Init();

        /// @brief GL VAO 핸들. @c glGenVertexArrays 로 할당, @c glDeleteVertexArrays 로 해제.
        GLuint mVertexArrayObject{0};
    };
}

#endif // __VERTEX_LAYOUT_H__
