#ifndef __SJH_COMMON_H__
#define __SJH_COMMON_H__

#pragma once

#include <memory>
#include <optional>
#include <string>

/**
 * @def CLASS_PTR
 * @brief 클래스 forward declaration + std 스마트 포인터 별칭을 일괄 생성하는 매크로.
 * @details 다음 3가지 typedef 를 한 번에 정의:
 *  - `<klassName>UPtr` — @c std::unique_ptr<klassName> (단독 소유)
 *  - `<klassName>Ptr`  — @c std::shared_ptr<klassName> (공유 소유)
 *  - `<klassName>WPtr` — @c std::weak_ptr<klassName>   (약한 참조)
 * @par 예시
 * @code
 * CLASS_PTR(Shader)  // ShaderUPtr / ShaderPtr / ShaderWPtr 자동 생성
 * @endcode
 * @note 모든 @c SJH:: 네임스페이스 클래스 헤더 상단에서 사용.
 */
#define CLASS_PTR(klassName)                            \
    class klassName;                                    \
    using klassName##UPtr = std::unique_ptr<klassName>; \
    using klassName##Ptr = std::shared_ptr<klassName>;  \
    using klassName##WPtr = std::weak_ptr<klassName>;

namespace SJH
{
    /**
     * @brief 텍스트 파일을 한 번에 읽어 @c std::string 으로 반환.
     * @param filename 읽을 파일의 경로 (실행 디렉토리 기준 상대 경로 또는 절대 경로).
     * @return 성공 시 파일 내용 문자열, 실패 시 @c std::nullopt.
     * @details 실패 사유(파일 없음/권한 등)는 @c spdlog::error 로 출력.
     *          GLSL 셰이더 소스 등 텍스트 리소스 로딩에 사용.
     */
    std::optional<std::string> LoadTextFile(const std::string &filename);

}

#endif //__SJH_COMMON_H__
