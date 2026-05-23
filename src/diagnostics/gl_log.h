#ifndef __SJH_DIAGNOSTICS_GL_LOG_H__
#define __SJH_DIAGNOSTICS_GL_LOG_H__

#include <GL/glcorearb.h>
#pragma once

#include "GL/gl3w.h"
#include <initializer_list>
#include <string_view>
#include <vector>

namespace SJH::Diagnostics
{
    /// @brief GL 객체(셰이더/프로그램) 상태 쿼리 + InfoLog 일원화 헬퍼.
    /// @details 컴파일/링크/검증은 비동기 아니지만 별도 status 쿼리가 유일한 검증 수단.
    ///          그 반복 보일러플레이트를 본 클래스에 가둔다.
    class GLObjectLog
    {
    public:
        /// @brief @c glCompileShader 직후 호출 — 실패 시 GLSL 컴파일러 InfoLog 를 error 출력.
        /// @param shader 검사할 셰이더 핸들
        /// @param tag    로그 식별자(예: 셰이더 파일 경로). 비우면 생략.
        /// @return 컴파일 성공 시 @c true.
        /// @note 사용 모듈: @c src/shader/
        static bool CheckShaderCompile(GLuint shader, std::string_view tag = {});

        /// @brief @c glLinkProgram 직후 호출 — 실패하면 uniform/attribute location 이 무효해지므로 필수.
        /// @return 링크 성공 시 @c true.
        /// @note 사용 모듈: @c src/program/ (Program 클래스 빌드 직후)
        static bool CheckProgramLink(GLuint program, std::string_view tag = {});

        /// @brief 현재 GL 상태(VAO/uniform/texture 바인딩 등)에서 프로그램 실행 가능성 검증.
        /// @details 내부적으로 @c glValidateProgram 호출 -> @c GL_VALIDATE_STATUS 확인.
        ///          단순 link 성공과 별개로 "지금 이 상태에서 draw 가능한가"를 드라이버에 위임.
        /// @return 검증 성공 시 @c true (실패는 @c warn 레벨 — 치명적은 아니나 의심 신호).
        /// @warning 무거운 호출 — 매 프레임 호출 금지. 개발/디버깅 빌드의 draw 직전에만.
        /// @note 사용 모듈: @c src/program/ 또는 renderer (디버그 빌드 한정)
        static bool CheckProgramValidate(GLuint program, std::string_view tag = {});

        /// @brief 프로그램에 기대되는 uniform 들이 모두 존재하는지 정적 검증.
        /// @details
        ///  - 각 name 에 대해 @c glGetUniformLocation < 0 검사 -> 누락된 것을 한 번에 spdlog::warn.
        ///  - **같은 program 핸들에 대해 최초 1회만** 실 검사. 이후 호출은 캐시된 bool 즉시 반환,
        ///    로그 출력 없음 -> 매 draw 호출 안에 두어도 부담 0.
        ///  - 누락은 warn 레벨 (error 아님) — 셰이더 옵티마이저가 inactive uniform 제거하는 게 정상이라서.
        /// @return 모든 uniform 존재 시 @c true.
        /// @note 사용 모듈: @c src/context/ 등 program 사용 시점.
        static bool CheckExpectedUniforms(
            GLuint program,
            std::initializer_list<const char *> names,
            std::string_view tag = {});

        /// @brief vertex attribute 동일 — @c glGetAttribLocation < 0 검사 + program 별 1회 캐시.
        /// @note 사용 모듈: @c src/layout/ 또는 program 사용 시점.
        static bool CheckExpectedAttributes(
            GLuint program,
            std::initializer_list<const char *> names,
            std::string_view tag = {});

        /// @brief 캐시 무효화 — @c Program 소멸자에서 호출.
        /// @details 같은 @c GLuint 핸들이 재발급될 때 stale 캐시 결과 방지.
        ///          @c CheckExpectedUniforms / @c CheckExpectedAttributes 의 program 별 1회 캐시를 정리.
        static void InvalidateProgramCache(GLuint program);
    };

    /// @brief GL 호출별 *명시적* 에러 검사 헬퍼 모음 (호출-당 1:1 진단).
    /// @details
    ///  - 각 함수는 *특정 GL 호출 직후* 에 호출하는 용도. 함수명이 곧 계약.
    ///  - 내부에서 @c glGetError 1회 호출 -> GL 명세상 가능한 enum 들을 case 별로 보고.
    ///  - 학습 단계에서 *어떤 호출이 어떤 에러를 낼 수 있는지* 코드에 명시되도록 풀어놓은 형태.
    ///  - 콜백/매크로 방식 (@c SJH_GL_CHECK) 대비 verbose 하지만 가르침 가치 큼.
    /// @note 모든 함수는 호출 직전 GL 큐가 비어있다고 가정 — 호출별 일관 적용 필수.
    class GLDebug
    {
    public:
        /// @brief @c glGenVertexArrays(n,*) 직후 호출.
        /// @details 명세상 가능 에러: @c GL_INVALID_VALUE (n<0).
        /// @return 에러 없으면 @c true, 있으면 spdlog::error 후 @c false.
        static bool CheckGLGenVertexArrays();

        /// @brief @c glBindVertexArray(vao) 직후 호출.
        /// @details 명세상 가능 에러: @c GL_INVALID_OPERATION
        ///          (vao 가 0이 아니면서 @c glGenVertexArrays 가 반환한 이름이 아닐 때).
        /// @param vao 진단 로그에 출력할 VAO 핸들.
        static bool CheckGLBindVertexArray(const GLuint vao);

        /// @brief @c glGenBuffers(n,*) 직후 호출.
        /// @details 명세상 가능 에러: @c GL_INVALID_VALUE (n<0).
        /// @param vbo 진단 로그에 출력할 버퍼 핸들 (생성된 값).
        static bool CheckGLGenBuffers(const GLuint vbo);

        /// @brief @c glBindBuffer(target,buf) 직후 호출.
        /// @details 명세상 가능 에러:
        ///  - @c GL_INVALID_ENUM — target 이 허용 enum 아님 (@c GL_ARRAY_BUFFER, @c GL_ELEMENT_ARRAY_BUFFER, ...).
        ///  - @c GL_INVALID_VALUE — buf 가 @c glGenBuffers 가 반환한 이름이 아님.
        /// @param vbo 진단 로그에 출력할 버퍼 핸들.
        static bool CheckGLBindBuffer(const GLuint vbo);

        /// @brief @c glBufferData(target,size,data,usage) 직후 호출.
        /// @details 명세상 가능 에러:
        ///  - @c GL_INVALID_ENUM — target 또는 usage 부적합.
        ///  - @c GL_INVALID_VALUE — size<0.
        ///  - @c GL_INVALID_OPERATION — 이름 0이 target 에 바인딩 (= @c glBindBuffer 안 함), 또는 mapped.
        ///  - @c GL_OUT_OF_MEMORY — GPU 메모리 부족.
        /// @param data_size 진단 로그에 출력할 업로드 size (bytes).
        static bool CheckGLBufferData(const size_t data_size);

        /// @brief @c glEnableVertexAttribArray(idx) 직후 호출.
        /// @details 명세상 가능 에러:
        ///  - @c GL_INVALID_OPERATION — VAO 미바인딩 (3.3 core 강제).
        ///  - @c GL_INVALID_VALUE — idx >= @c GL_MAX_VERTEX_ATTRIBS (보통 16).
        /// @param layout_location 진단 로그에 출력할 attribute index.
        static bool CheckGLEnableVertexAttribArray(GLuint layout_location);

        /// @brief @c glVertexAttribPointer(idx,size,type,...) 직후 호출.
        /// @details 명세상 가능 에러:
        ///  - @c GL_INVALID_VALUE — idx>=max, size 가 {1,2,3,4,GL_BGRA} 아님, stride<0.
        ///  - @c GL_INVALID_ENUM — type 부적합.
        ///  - @c GL_INVALID_OPERATION — VAO 미바인딩, 또는 non-zero offset 인데 VBO 미바인딩.
        /// @param strides 진단 로그에 출력할 stride 리스트 (호출별 stride 컨텍스트 캡처).
        static bool CheckGLVertexAttribPointer(const std::vector<GLsizei>&& strides);
    };
}

#endif // __SJH_DIAGNOSTICS_GL_LOG_H__
