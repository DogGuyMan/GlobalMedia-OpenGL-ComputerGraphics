/**
 * @file effekseer_diagnostics.h
 * @brief Effekseer .efk 버전/텍스처 경로 트랩 + Play 핸들 진단 (2026-06-02 신설).
 *
 * @details
 *  ### 책임
 *  - @c CheckEffectTextures - .efk 바이너리에서 UTF-16LE 런 스캔으로 참조 리소스 경로를 추출,
 *    base 디렉토리 기준 실제 존재 여부 검사. 텍스처/모델/머티리얼 누락 조기 탐지.
 *  - @c CheckPlayHandle - @c Manager::Play() 반환 핸들 음수 검사 (재생 실패 즉시 탐지).
 *  - @c CheckHandleAlive - Play 직후 @c Manager::Exists() 가 @c false 인 경우 탐지
 *    (빈 이펙트 / 텍스처 전무 -> 즉시 종료 패턴).
 *
 *  ### 비-책임
 *  - [X] GL 상태 진단 - @c gl_log.h / @c gl_validate.h.
 *  - [X] Effekseer Manager/Renderer 생성 - @c VFXSystem(Client 거주).
 *  - [X] .efk 버전 포맷 검증 - 본 모듈은 magic(@c 'SKFE') 확인만. 버전 필드 파싱 불포함.
 *
 *  ### 의존성
 *  GL/Effekseer SDK 완전 비의존 - 파일 I/O(@c std::filesystem) + 정수/불리언만 사용.
 *  모든 출력은 @c spdlog::warn / @c spdlog::info - 빌드/실행을 막지 않는 진단 신호.
 *
 *  ### 관련 트랩 (memory/efk_texture_basepath_trap.md)
 *  - 텍스처 base 경로: 내장 상대경로는 .efk 디렉토리 기준 (@c ../ 는 부모).
 *  - 버전 mismatch: Effekseer 1.7 에디터로 export 하지 않으면 @c Effect::Create 가 조용히 null 반환.
 *
 * @note 챕터는 관용적으로 `namespace diag = SJH::Diagnostics;` 별칭 사용.
 */

#ifndef __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__

#include <cstdint>
#include <string_view>

namespace SJH::Diagnostics
{
    /**
     * @brief Effekseer 생성/사용 디버깅 진단 - GL/Effekseer SDK 비의존 순수 유틸.
     * @details
     *  순수 static 유틸 클래스. @c GLDebug / @c UniformDiagnostics 컨벤션과 동일하게
     *  인스턴스화 금지. 모든 진단은 @c spdlog::warn / @c spdlog::info 로만 출력.
     */
    class EffekseerDiagnostics
    {
    public:
        // 순수 static 유틸 - 인스턴스화 금지 (UniformDiagnostics 컨벤션과 동일).
        EffekseerDiagnostics()                                        = delete;
        EffekseerDiagnostics(const EffekseerDiagnostics &)            = delete;
        EffekseerDiagnostics &operator=(const EffekseerDiagnostics &) = delete;

        /// @brief .efk 가 참조하는 텍스처/모델/머티리얼이 base 기준 실제 존재하는지 검증.
        /// @details .efk 바이너리에서 UTF-16LE 런 스캔으로 참조 경로를 추출한 뒤
        ///          @p baseDir 를 기준으로 @c std::filesystem::exists 로 확인.
        ///          Effekseer SDK 비의존 - 로드 전/후 어느 시점이나 호출 가능.
        ///          누락된 참조는 각각 @c spdlog::warn 으로 경로 출력.
        /// @param efkPath .efk 파일 경로 (UTF-8). 슬래시 통일 권장.
        /// @param baseDir 텍스처 해석 기준 디렉토리. 비우면 @p efkPath 의 부모 디렉토리 사용
        ///                (= @c Effekseer::Effect::Create 에서 materialPath 없을 때 동작과 동일).
        /// @return 누락된 참조 수 (0 = 전부 확인됨). 파일 열기 실패 = @c -1,
        ///         magic(@c 'SKFE') 불일치 = @c -2.
        static int CheckEffectTextures(std::string_view efkPath, std::string_view baseDir = {});

        /// @brief @c Manager::Play() 반환 핸들 유효성 검사 - Play 호출 직후 사용.
        /// @details 핸들 음수(@c Effekseer::Handle < 0) 는 재생 실패 신호.
        ///          manager/effect null 또는 maxSprites 초과 시 음수 반환.
        /// @param handle @c Play() 반환값 (@c Effekseer::Handle = @c int32_t). 음수 = 재생 실패.
        /// @param tag    로그 식별자(effect key 등). 비우면 생략.
        /// @return 유효(@c >= 0) 시 @c true. 실패 시 @c spdlog::warn 후 @c false.
        static bool CheckPlayHandle(int32_t handle, std::string_view tag = {});

        /// @brief Play 직후 이펙트 인스턴스가 즉시 소멸했는지 감지.
        /// @details 빈 이펙트(텍스처 전무) 또는 버전 mismatch 시 Play 직후 @c Exists() 가 @c false 가 됨.
        ///          @c handle < 0 이면 @c CheckPlayHandle 이 이미 보고했으므로 조용히 @c false 반환.
        /// @param handle @c Play() 반환 핸들.
        /// @param exists 호출자가 @c mManager->Exists(handle) 로 계산해 전달.
        /// @param tag    로그 식별자.
        /// @return @c handle >= 0 && @c exists 면 @c true.
        ///         handle 유효한데 @c !exists 면 @c spdlog::warn 후 @c false.
        static bool CheckHandleAlive(int32_t handle, bool exists, std::string_view tag = {});
    };
}

#endif // __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
