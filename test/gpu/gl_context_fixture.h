/**
 * @file gl_context_fixture.h
 * @brief 전 GPU 테스트가 공유하는 세션 1회 GL 컨텍스트 픽스처 (Phase C C-0).
 *
 * @details
 *  ### 책임
 *  - 숨김 GLFW 창 + GL 4.1 Core 컨텍스트를 *테스트 세션 시작 시 단 1회* 생성하고,
 *    세션 종료 시 정리한다. (컨텍스트 생성은 비싸고 스레드 비안전 -> 1회만.)
 *  - GL 로더(gl3w) 초기화까지 마쳐, 이후 모든 GPU 테스트 본문은 컨텍스트가
 *    유효하다고 가정하고 GL 호출을 바로 쓸 수 있다.
 *
 *  ### 구조
 *  Catch2 글로벌 이벤트 리스너(@c Catch::EventListenerBase) 의
 *  @c testRunStarting / @c testRunEnded 훅에서 init/shutdown 한다.
 *  커스텀 main 대신 리스너를 쓰면 @c Catch2::Catch2WithMain 의 표준 main 을
 *  그대로 재사용하면서 세션 경계에 1회 init 을 끼울 수 있다(spec 허용 옵션).
 *
 *  ### 프로젝트 관행
 *  - 로더 = gl3w (@c gl3wInit, 심볼은 sb7 정적 라이브러리에 거주).
 *  - 인클루드 순서 = @c GL/gl3w.h 먼저, @c GLFW_INCLUDE_NONE 정의 후 @c GLFW/glfw3.h
 *    (sb7.h 와 동일 - gl3w/GLFW 간 gl3.h 중복 포함 방지).
 *  - GLFW 는 sb7 vendored 3.0.4 라 @c GLFW_TRUE / @c GLFW_FALSE 매크로가 없다.
 *    -> @c GL_TRUE / @c GL_FALSE (glcorearb.h, 1/0) 리터럴 사용.
 */

#ifndef __SJH_TEST_GPU_GL_CONTEXT_FIXTURE_H__
#define __SJH_TEST_GPU_GL_CONTEXT_FIXTURE_H__

#include "GL/gl3w.h"
#define GLFW_INCLUDE_NONE 1
#include "GLFW/glfw3.h"

namespace SJH::Test::Gpu
{
    /// @brief 세션 GL 컨텍스트의 전역 핸들 보관 + init/shutdown 일원화.
    /// @details 모든 GPU 테스트가 공유하는 단일 상태. 리스너가 소유권을 갖는다.
    class GLContextFixture
    {
    public:
        /// @brief 세션 1회 컨텍스트 생성 (이미 유효하면 no-op).
        /// @return 컨텍스트 + 로더 init 성공 시 true. 실패 시 false (헤드리스/드라이버 부재).
        static bool Init();

        /// @brief 세션 종료 시 컨텍스트/창/GLFW 정리.
        static void Shutdown();

        /// @brief 현재 컨텍스트가 유효(생성 완료)한지.
        static bool IsValid();

    private:
        static GLFWwindow *sWindowPtr; ///< 숨김 창 핸들 (컨텍스트 owner).
        static bool        sInitialized;
    };
}

#endif // __SJH_TEST_GPU_GL_CONTEXT_FIXTURE_H__
