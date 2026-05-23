#ifndef __SJH_SHADER_H__
#define __SJH_SHADER_H__

// 가드 이름은 __SJH_SHADER_H__ — sb7code 의 include/shader.h 가 __SJH_SHADER_H__ 를
// 선점하므로 동일 가드 사용 시 sb7::shader::load 가 보이지 않는 버그 회피.

#include "common/common.h"
#include "GL/gl3w.h"

namespace SJH
{
    CLASS_PTR(Shader)

    /**
     * @brief OpenGL 셰이더 객체(@c GL_VERTEX_SHADER / @c GL_FRAGMENT_SHADER 등) 의 RAII 래퍼.
     * @details 팩토리 함수 @ref CreateFromFile 으로만 인스턴스 생성 가능.
     *          외부에 노출되는 인스턴스는 항상 컴파일까지 완료된 유효한 GL 핸들을 보유한다는
     *          불변식을 가진다. 소멸자에서 @c glDeleteShader 자동 호출.
     */
    class Shader
    {
    public:
        /**
         * @brief 파일에서 셰이더 소스를 읽어 컴파일 후 @c Shader 인스턴스 생성.
         * @param filename     셰이더 소스 파일 경로 (예: @c "resources/shader/simple.vs").
         * @param shader_type  GL 셰이더 타입 (@c GL_VERTEX_SHADER, @c GL_FRAGMENT_SHADER 등).
         * @return 성공 시 @c ShaderUPtr (소유권 이전), 실패(파일 없음/컴파일 에러) 시 @c nullptr.
         * @details 팩토리 패턴이 다음 3가지를 보장한다:
         *  -# **예외 없는 실패 처리** — 생성자는 실패를 신호할 방법이 없으므로 팩토리가 @c nullptr 반환으로 처리.
         *  -# **RAII 소유권 강제** — 생성자 @c private + 반환 타입 @c UPtr 의 협력:
         *     생성자 private 으로 직접 생성 차단, @c UPtr 반환으로 호출자에게 자동 소유권 이전.
         *  -# **클래스 불변식** — 외부 노출 인스턴스는 항상 유효한 GL 핸들 보유:
         *     팩토리 내부에서 빈 객체 생성 -> @c TryLoadFile 로 GL 자원 획득 시도 ->
         *     성공 시 소유권 이전, 실패 시 임시 UPtr 즉시 파괴 + @c nullptr 반환.
         * @note 컴파일 에러 로그는 @c diagnostics::GLObjectLog::CheckShaderCompile 가 출력.
         */
        static ShaderUPtr CreateFromFile(const std::string &filename, GLenum shader_type);

        /**
         * @brief 인라인 소스 문자열에서 직접 컴파일 후 @c Shader 인스턴스 생성.
         * @param source        GLSL 소스 코드 (`#version` 디렉티브 포함 권장).
         * @param shader_type   GL 셰이더 타입.
         * @return 성공 시 @c ShaderUPtr, 컴파일 실패 시 @c nullptr.
         * @details 파일 I/O 우회 — 테스트의 인라인 GLSL 또는 *런타임 생성 셰이더* 용도.
         *          @c CreateFromFile 과 같은 팩토리 불변식 보장 (외부 노출 인스턴스는 컴파일 완료).
         */
        static ShaderUPtr CreateFromSource(const std::string &source, GLenum shader_type);

        /// @brief @c glDeleteShader 호출 (핸들이 0 이 아닐 때만).
        ~Shader();

        // SP1 — 자원 핸들 이중 해제 차단. 팩토리 + UPtr 패턴이므로 외부에서
        //       복사,이동할 경로가 애초에 없음.
        Shader(const Shader&)            = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&)                 = delete;
        Shader& operator=(Shader&&)      = delete;

        /// @brief 내부 GL 셰이더 핸들 반환 — @c Program::Create 가 attach 시 사용.
        GLuint GetShaderAddr() const { return mShaderAddr; }

    private:
        Shader() = default;
        bool TryLoadFile(const std::string &filename, GLenum shader_type);
        GLuint mShaderAddr{0};
    };
}
#endif // __SJH_SHADER_H__
