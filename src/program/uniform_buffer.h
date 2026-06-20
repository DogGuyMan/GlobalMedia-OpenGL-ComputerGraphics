/**
 * @file uniform_buffer.h
 * @brief @c GL_UNIFORM_BUFFER 객체의 RAII 래퍼 - Slang 생성 std140 UBO 블록 1개의 GPU 백킹 스토리지.
 *
 * @details
 *  ### 책임 (Phase 2 T2)
 *  - @c glGenBuffers / @c glBufferData (@c GL_DYNAMIC_DRAW, 고정크기) 로 GL UBO 객체 생성.
 *  - @c Update 로 @c glBufferSubData 부분 갱신 - std140 layout offset 은 호출자가 계산.
 *  - @c BindBase 로 @c glBindBufferBase 결속 - 셰이더 측 @c layout(std140) block 과 binding point 연결.
 *  - 소멸자에서 @c glDeleteBuffers 자동 호출 (RAII).
 *
 *  ### 비-책임
 *  - [X] UBO 블록 introspection / 자기기술 - @c Program::UniformBlock (T3) 이 owner 로서 담당.
 *  - [X] std140 layout offset 계산 - 셰이더 컴파일러(Slang) 가 결정, refl.json 또는 호출자가 사전 계산.
 *  - [X] 셰이더 측 block 선언 - .slang 의 @c ConstantBuffer<T> 가 GLSL @c layout(std140) uniform block 으로 변환됨.
 *
 *  ### 거주 위치 결정 (사이클 회피)
 *  - @c SJH::buffer 모듈은 이미 @c SJH::program 을 @c PUBLIC 의존 (@c src/buffer/CMakeLists.txt:17).
 *  - UBO 를 @c src/buffer/ 에 두면 *유일 소비자* 인 @c Program 이 다시 buffer 를 끌어와야 해
 *    @c program ↔ buffer 양방향 사이클 발생.
 *  - @c Program 이 UBO 의 유일 소비자이고 buffer 가 program 을 이미 의존하므로 transitively 가시.
 *  - → @c src/program/ 거주가 사이클 회피 + 단일 소유자 모듈 거주 정통.
 *
 *  ### 팩토리 패턴
 *  - 외부 노출 인스턴스는 항상 GL 객체 생성 완료 상태 - 기본 생성자 @c private.
 *  - @c Create 정적 팩토리만 허용. 복사/이동 @c = delete (RAII 핸들 이중 해제 차단).
 *
 * @note Phase 2 의 D11 (Frame/Draw/Material 3분할) 에서 블록당 UBO 1개 생성.
 *       Program::UniformBlock 이 @c UniformBufferUPtr 멤버로 소유.
 */
#ifndef __SJH_UNIFORM_BUFFER_H__
#define __SJH_UNIFORM_BUFFER_H__

#include "common/common.h"
#include "GL/gl3w.h"
#include <cstddef>

namespace SJH
{
    CLASS_PTR(UniformBuffer)

    /**
     * @brief GL_UNIFORM_BUFFER 객체의 RAII 래퍼 - 고정크기 할당 후 부분갱신 / binding-point 결속.
     * @details Slang 의 @c ConstantBuffer<T> 가 GLSL 출력에서 @c layout(std140) uniform block_<T>_0 로
     *          변환되는데, 그 GPU 백킹 스토리지 1:1 매핑. T 의 std140 size 만큼 1회 할당 후
     *          @c Update 로 부분/전체 갱신.
     */
    class UniformBuffer
    {
    public:
        /**
         * @brief @p size 바이트 크기의 UBO 객체 생성 (@c GL_DYNAMIC_DRAW).
         * @param size std140 layout 기준 블록 전체 바이트 크기 (호출자가 @c GL_UNIFORM_BLOCK_DATA_SIZE 또는
         *             refl.json 으로 사전 계산).
         * @return 성공 시 @c UniformBufferUPtr, @c glGenBuffers 실패 시 @c nullptr.
         * @note GL context 활성 상태에서만 호출 유효 (gl3w 초기화 이후).
         */
        static UniformBufferUPtr Create(std::size_t size);

        /// @brief @c glDeleteBuffers 호출 (핸들이 0 이 아닐 때만).
        ~UniformBuffer();

        // RAII 핸들 이중 해제 차단 - 팩토리 + UPtr 패턴이므로 외부에서 복사/이동 경로 없음.
        UniformBuffer(const UniformBuffer&)            = delete;
        UniformBuffer& operator=(const UniformBuffer&) = delete;
        UniformBuffer(UniformBuffer&&)                 = delete;
        UniformBuffer& operator=(UniformBuffer&&)      = delete;

        /**
         * @brief UBO 의 @p offset 바이트 위치에 @p data 를 @p bytes 만큼 업로드.
         * @param data 업로드할 CPU 측 메모리 시작 포인터 (std140 layout 정합 가정).
         * @param bytes 업로드할 바이트 수.
         * @param offset UBO 내 시작 오프셋 (멤버별 std140 offset 은 호출자가 계산).
         * @details 내부적으로 @c glBindBuffer (@c GL_UNIFORM_BUFFER) + @c glBufferSubData + unbind 수행.
         *          T4 의 D11 분할 패턴 - 예: @c FrameBlock 의 @c uProj 갱신 시 @c offset=sizeof(mat4).
         */
        void Update(const void* data, std::size_t bytes, std::size_t offset = 0) const;

        /**
         * @brief UBO 를 @p bindingPoint 에 결속 (@c glBindBufferBase).
         * @param bindingPoint 셰이더 측 @c layout(binding=N) 또는 @c glUniformBlockBinding 으로 매핑한 인덱스.
         * @details 드로우 콜 전 1회 호출 - 셰이더의 block 이 본 UBO 의 데이터를 읽어가는 채널이 열린다.
         */
        void BindBase(GLuint bindingPoint) const;

        /// @brief 내부 GL 버퍼 핸들 반환 - 진단 / 외부 GL 호출 시 키.
        GLuint Get() const { return mBuffer; }

        /// @brief 할당된 바이트 크기 (@c Create 시점 @c size 인자).
        std::size_t Size() const { return mSize; }

    private:
        UniformBuffer() = default;

        /// @brief @c glGenBuffers + @c glBufferData (DYNAMIC_DRAW) 수행. 실패 시 false.
        bool Init(std::size_t size);

        /// @brief 내부 GL 버퍼 핸들 - @c glDeleteBuffers 대상.
        GLuint      mBuffer{0};

        /// @brief 할당된 바이트 크기 - @c Update 의 bytes+offset 가드용 (현 구현은 GL 드라이버에 위임).
        std::size_t mSize{0};
    };
}

#endif // __SJH_UNIFORM_BUFFER_H__
