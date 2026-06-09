/**
 * @file shader.h
 * @brief OpenGL 셰이더 객체(@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등) RAII 래퍼.
 *
 * @details
 *  ### 책임
 *  - **셰이더 컴파일** - 파일 경로(@c CreateFromFile) 또는 인라인 소스 문자열(@c CreateFromSource)
 *    에서 @c glCreateShader + @c glCompileShader 까지 일괄 수행.
 *  - **InfoLog 추출** - 컴파일 실패 시 @c Diagnostics::GLObjectLog::CheckShaderCompile 로
 *    InfoLog 를 spdlog 에 출력.
 *  - **RAII 생애주기** - 소멸자에서 @c glDeleteShader 자동 호출.
 *    외부에 노출된 인스턴스는 *항상 컴파일 완료된 유효한 핸들* 이라는 불변식 유지.
 *
 *  ### 비-책임
 *  - [X] 프로그램 링킹 (@c glAttachShader / @c glLinkProgram) - @c SJH::Program (@c src/program/) 담당.
 *  - [X] uniform 전송 - @c SJH::Program::SetUniform* 담당.
 *  - [X] 셰이더 소스 핫-리로드 - 현재 1회 컴파일 후 핸들 고정.
 *
 * @note 헤더 가드 이름이 @c __SJH_SHADER_H__ 인 이유:
 *       @c sb7code 의 @c include/shader.h 가 동일 가드를 선점하므로, 가드 충돌 시
 *       @c sb7::shader::load 심볼이 숨겨지는 버그를 회피하기 위해 의도적으로 동일한 이름 사용.
 *       (sb7 포함 순서 의존성 주의)
 */
#ifndef __SJH_SHADER_H__
#define __SJH_SHADER_H__

#include "common/common.h"
#include "GL/gl3w.h"

namespace SJH
{
    CLASS_PTR(Shader)

    /**
     * @brief OpenGL 셰이더 객체(@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등) RAII 래퍼.
     * @details
     *  팩토리 함수 @c CreateFromFile 또는 @c CreateFromSource 로만 인스턴스 생성 가능.
     *  외부에 노출되는 인스턴스는 *항상 컴파일까지 완료된 유효한 GL 핸들* 을 보유한다는
     *  불변식을 유지한다. 소멸자에서 @c glDeleteShader 자동 호출.
     *
     *  ### 팩토리 불변식 3가지
     *  -# **예외 없는 실패 처리** - 생성자는 실패를 신호할 수 없으므로 팩토리가 @c nullptr 반환.
     *  -# **RAII 소유권 강제** - 생성자 @c private + 반환 타입 @c UPtr:
     *     직접 생성 차단, @c UPtr 반환으로 호출자에게 자동 소유권 이전.
     *  -# **클래스 불변식** - 빈 객체 생성 -> GL 자원 획득 시도 ->
     *     성공 시 소유권 이전, 실패 시 임시 UPtr 즉시 파괴 + @c nullptr 반환.
     */
    class Shader
    {
    public:
        /**
         * @brief 파일에서 셰이더 소스를 읽어 컴파일 후 @c Shader 인스턴스 생성.
         * @param filename     셰이더 소스 파일 경로 (예: @c "resources/shaders/simple.vert").
         * @param shader_type  GL 셰이더 타입 (@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등).
         * @return 성공 시 @c ShaderUPtr (소유권 이전), 실패(파일 없음/컴파일 에러) 시 @c nullptr.
         * @details 내부적으로 @c sb7::shader::load (파일 IO + @c glCreateShader +
         *          @c glShaderSource + @c glCompileShader 일괄 수행) 를 경유한다.
         *          컴파일 에러 InfoLog 는 @c Diagnostics::GLObjectLog::CheckShaderCompile 가 spdlog 에 출력.
         */
        static ShaderUPtr CreateFromFile(const std::string &filename, GLenum shader_type);

        /**
         * @brief 인라인 소스 문자열에서 직접 컴파일 후 @c Shader 인스턴스 생성.
         * @param source        GLSL 소스 코드 (@c #version 디렉티브 포함 권장).
         * @param shader_type   GL 셰이더 타입 (@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등).
         * @return 성공 시 @c ShaderUPtr, 컴파일 실패 시 @c nullptr.
         * @details 파일 I/O 우회 - 단위 테스트의 인라인 GLSL 또는 런타임 생성 셰이더 용도.
         *          @c CreateFromFile 과 동일한 팩토리 불변식 보장. 진단 tag 는 빈 문자열.
         */
        static ShaderUPtr CreateFromSource(const std::string &source, GLenum shader_type);

        /// @brief @c glDeleteShader 호출 (핸들이 0 이 아닐 때만).
        ~Shader();

        // SP1 - GL 핸들 이중 해제 차단. 팩토리 + UPtr 패턴으로 외부 복사/이동 경로 없음.
        Shader(const Shader &)            = delete;
        Shader &operator=(const Shader &) = delete;
        Shader(Shader &&)                 = delete;
        Shader &operator=(Shader &&)      = delete;

        /// @brief 내부 GL 셰이더 핸들 반환 - @c Program::Create 가 @c glAttachShader 시 사용.
        GLuint GetShaderAddr() const { return mShaderAddr; }

    private:
        Shader() = default;

        /**
         * @brief 파일 경로에서 셰이더를 로드/컴파일하고 @c mShaderAddr 에 결과 저장.
         * @param filename     셰이더 소스 파일 경로.
         * @param shader_type  GL 셰이더 타입.
         * @return 컴파일 성공 시 @c true, 실패 시 @c false (InfoLog spdlog 출력).
         * @details @c sb7::shader::load 경유 - 멤버 @c mShaderAddr 에 직접 대입
         *          (지역 변수 shadow 방지).
         */
        bool TryLoadFile(const std::string &filename, GLenum shader_type);

        /// @brief GL 셰이더 핸들. @c glCreateShader 로 할당, @c glDeleteShader 로 해제.
        GLuint mShaderAddr{0};
    };
}
#endif // __SJH_SHADER_H__
