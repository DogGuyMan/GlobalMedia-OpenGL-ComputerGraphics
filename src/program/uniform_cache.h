/**
 * @file uniform_cache.h
 * @brief 셰이더 active uniform 의 name -> (location, type) 캐시 - @c Program 의 부수 객체.
 *
 * @details
 *  ### 책임 (SP6)
 *  - link 완료된 @c Program 의 active uniform 전체 enumerate (@c glGetActiveUniform).
 *  - name 키로 @c GLint location + @c GLenum type 의 빠른 lookup 제공.
 *  - @c Material / @c PropertyBlockSetter 가 @c Program 참조 시 이 캐시를 *조회* -
 *    셰이더 schema 를 자기 단위로 인식할 수 있게 함.
 *
 *  ### 비-책임
 *  - [X] uniform 값 설정 - @c SJH::Uniforms 자유 함수 family (@c program_uniforms.h) 담당.
 *  - [X] @c glUseProgram 바인딩 관리 - @c DeviceContext 담당.
 *  - [X] dead-code elimination 된 비-active uniform 수집 - @c glGetActiveUniform 이 원래 제외.
 *
 *  ### 분리 이유 (SP6 D-5)
 *  - SP1~SP5: @c Program 의 멤버 unordered_map - Program 이 *GL 핸들* 과 *schema* 책임 동시 보유.
 *  - SP6: 별도 클래스로 분리 -> @c Program 의 SRP 회복 + @c Material 의 *셰이더 schema 자기 단위* 가능.
 *  - 소유자는 여전히 @c Program - @c Material 은 @c const UniformCache* 로 reference.
 *    dangling 방지는 @c Program::~Program 의 Observer cascade (D-6) 가 담당.
 *
 * @note 배열 원소 (@c "arr[3]" 등 비-canonical 이름) 는 @c glGetActiveUniform 결과에 없으므로
 *       @c GetLocation 이 @c -1 반환 -> 호출자(@c Uniforms setter)가 @c glGetUniformLocation fallback.
 */
#ifndef __SJH_UNIFORM_CACHE_H__
#define __SJH_UNIFORM_CACHE_H__

#include "GL/gl3w.h"
#include <string>
#include <unordered_map>

namespace SJH
{
    class Program;   // forward - Build 가 사용하는 GetProgramAddr() 만.

    /// @brief active uniform 의 name -> (location, type) 캐시.
    class UniformCache
    {
    public:
        /// @brief uniform 단위 entry. SP1~SP5 시점의 Program::UniformEntry 와 동일 형식.
        struct Entry { GLint Location; GLenum Type; };

        UniformCache() = default;

        /// @brief link 완료된 Program 의 active uniform 전체 수집 (eager build).
        /// @details glGetActiveUniform 으로 enumerate - 비-active uniform (dead-code elim) 은 제외.
        ///          호출 전 mEntries 가 비어 있다고 가정 - 재호출 시 Clear() 먼저.
        void Build(const Program& prog);

        /// @brief 캐시 비우기 - 명시 invalidate 가 필요할 때만 (예: 셰이더 재링크).
        ///        Program 멤버이므로 ~Program 시 자동 destroy - 일반 호출 불요.
        void Clear() { mEntries.clear(); }

        /// @brief uniform 이름 -> location. 미존재 시 -1 (호출자가 glGetUniformLocation fallback).
        /// @note  본 함수는 pure const query - cache mutation 없음 (POLA).
        GLint  GetLocation(const char* name) const;

        /// @brief uniform 이름 -> GL 타입 (GL_FLOAT_MAT4 등). 미존재 시 0.
        ///        진단의 타입 불일치 체크에 사용.
        GLenum GetType(const char* name) const;

        /// @brief 캐시된 active uniform 개수.
        std::size_t Size() const { return mEntries.size(); }

        /// @brief 모든 active uniform entries 의 const view (name -> Entry).
        /// @details `PropertyBlockSetter::Set` 의 *cache outer iteration* 용 - 셰이더 schema 가
        ///          진실의 원천. Material 의 properties bag 은 *value* 만 들고 있으므로
        ///          *어떤 uniform 이 존재하는지* 는 이 view 가 답한다.
        const std::unordered_map<std::string, Entry>& Entries() const { return mEntries; }

    private:
        std::unordered_map<std::string, Entry> mEntries;
    };
}

#endif // __SJH_UNIFORM_CACHE_H__
