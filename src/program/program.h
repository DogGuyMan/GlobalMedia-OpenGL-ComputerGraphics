/**
 * @file program.h
 * @brief OpenGL 프로그램 객체 RAII 래퍼 - 셰이더 attach + link + uniform 캐시 소유.
 *
 * @details
 *  ### 책임
 *  - 컴파일된 @c Shader 들을 attach + link 하여 GL 프로그램 핸들을 생성.
 *  - link 성공 직후 @c UniformCache 의 eager build 수행 - active uniform schema 를 program 수명과 함께 보유.
 *  - 소멸자에서 @c glDeleteProgram 자동 호출 (RAII).
 *  - uniform location / type 을 @c GetLocation / @c GetType 으로 외부 query 에 노출.
 *
 *  ### 비-책임
 *  - [X] uniform 값 설정 - @c SJH::Uniforms 자유 함수 family (@c program_uniforms.h) 가 담당.
 *  - [X] `glUseProgram` 바인딩 관리 - @c DeviceContext 가 담당.
 *  - [X] 셰이더 소스 로드 / 컴파일 - @c Shader::CreateFromFile 이 담당.
 *
 *  ### 팩토리 패턴
 *  - 외부 노출 인스턴스는 항상 link 완료 상태 - 기본 생성자 @c private.
 *  - @c Create / @c CreateWithVSFS 두 정적 팩토리만 허용. 복사/이동 @c = delete.
 *
 * @note 캐시 분리 이력 (SP6): SP1~SP5 에서는 Program 멤버 unordered_map 이었으나
 *       SP6 에서 @c UniformCache 독립 클래스로 분리 -> SRP 회복.
 *       Program 이 여전히 owner - Material 은 @c const UniformCache* 로 reference.
 */

#ifndef __SJH_PROGRAM_H__
#define __SJH_PROGRAM_H__

#include "common/common.h"
#include "program/uniform_cache.h"   // SP6 - UniformCache 분리
#include "shader/shader.h"
#include "GL/gl3w.h"
#include <string>
#include <vector>

namespace SJH
{

    CLASS_PTR(Program)

    /**
     * @brief 컴파일된 셰이더들을 attach + link 한 OpenGL 프로그램 객체의 RAII 래퍼.
     * @details 팩토리 함수 @ref Create 으로만 인스턴스 생성 가능.
     *          외부 노출 인스턴스는 항상 링크까지 완료된 유효한 GL 핸들을 보유한다.
     *          소멸자에서 @c glDeleteProgram 자동 호출.
     *
     *  ### Uniform 캐시 - Program 의 *멤버* (SP2 완료, Pattern Y)
     *  - 캐시는 @c mUniformCache (@c UniformCache 멤버) - resource-attached (SP6 분리).
     *  - @c Create 가 link 성공 후 @c UniformCache::Build 호출 (eager build).
     *  - 소멸자에서 멤버가 자동 destroy - 별도 정리 호출 불요.
     *  - 호출 패턴: @c Uniforms::SetMat4(*prog, "name", data) - 자유 함수가 @c prog.GetLocation 경유.
     *  @see SJH::Uniforms
     *  @see SJH::UniformCache
     */
    class Program
    {
    public:
        /**
         * @brief 셰이더 벡터를 받아 프로그램 생성 + attach + link 일괄 수행.
         * @param shaders 링크할 셰이더 (vertex / fragment 등 - 보통 2~3개). @c ShaderPtr (shared) 사용.
         * @return 성공 시 @c ProgramUPtr (uniform 캐시 빌드 완료 상태), 링크 실패 시 @c nullptr.
         * @note 링크 에러 로그는 @c Diagnostics::GLObjectLog::CheckProgramLink 가 출력.
         * @see Shader::CreateFromFile
         */
        static ProgramUPtr Create(const std::vector<ShaderPtr> &shaders);

        /**
         * @brief VS/FS 파일 경로 2개로부터 직접 Program 생성하는 편의 팩토리.
         * @param vertShaderFilename 정점 셰이더 GLSL 파일 경로 (예: @c "resources/shaders/lighting.vert").
         * @param fragShaderFilename 프래그먼트 셰이더 GLSL 파일 경로.
         * @return 두 셰이더 컴파일 + 프로그램 link 모두 성공 시 @c ProgramUPtr, 실패 시 @c nullptr.
         * @details 내부적으로 @c Shader::CreateFromFile 2회 호출 후 @c Create 에 위임.
         *          호출자가 @c Shader 인스턴스를 따로 보관할 필요 없을 때 사용 (대부분의 경우).
         * @see Create, Shader::CreateFromFile
         */
        static ProgramUPtr CreateWithVSFS(const std::string& vertShaderFilename, const std::string& fragShaderFilename);

        /// @brief @c glDeleteProgram 호출 (핸들이 0 이 아닐 때만). @c mUniformCache 는 멤버 자동 destroy.
        ~Program();

        // SP1 - 자원 핸들 이중 해제 차단. 팩토리 + UPtr 패턴이므로 외부에서
        //       복사,이동할 경로가 애초에 없음.
        Program(const Program&)            = delete;
        Program& operator=(const Program&) = delete;
        Program(Program&&)                 = delete;
        Program& operator=(Program&&)      = delete;

        /// @brief 내부 GL 프로그램 핸들 반환 - @c glUseProgram / @c Uniforms 자유 함수의 키.
        GLuint GetProgramAddr() const { return mProgramAddr; }

        /// @brief uniform 이름 -> location 조회 (@c UniformCache 위임).
        /// @param name 셰이더 내 uniform 이름 (null-terminated).
        /// @return active uniform 이면 location, 미캐시 시 -1 (호출자가 @c glGetUniformLocation fallback).
        /// @note pure const query - cache mutation 없음. 배열 원소(`arr[3]` 등)는 캐시에 없어 -1 반환.
        GLint  GetLocation(const char* name) const { return mUniformCache.GetLocation(name); }

        /// @brief uniform 이름 -> GL 타입 (@c GL_FLOAT_MAT4 등). 미캐시 시 @c 0.
        /// @details @c Diagnostics::UniformDiagnostics::NotifyTypeMismatch 의 비교 기준으로 사용.
        GLenum GetType(const char* name) const { return mUniformCache.GetType(name); }

        /// @brief active uniform schema 의 const view - @c Material / @c PropertyBlockSetter 가 참조.
        /// @return Program 수명과 동일한 @c UniformCache const ref.
        const UniformCache& GetUniformCache() const { return mUniformCache; }

    private:
        Program() = default;

        /// @brief @c glCreateProgram + attach + @c glLinkProgram 수행. 실패 시 InfoLog 출력 후 false.
        bool TryLink(const std::vector<ShaderPtr> &shaders);

        /// @brief 내부 GL 프로그램 핸들 - @c glDeleteProgram 대상이자 @c glUseProgram 인자.
        GLuint mProgramAddr{0};

        /// @brief active uniform name -> (location, type) 캐시. Program 이 owner (SP6 분리).
        UniformCache mUniformCache;
    };
}

#endif // __SJH_PROGRAM_H__
