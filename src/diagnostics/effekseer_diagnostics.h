#ifndef __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
#define __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__

#include <cstdint>
#include <string_view>

namespace SJH::Diagnostics
{
    /// @brief Effekseer 생성·사용 디버깅 진단 (의존성 0 — 파일 I/O + 정수/불리언만).
    /// @details GLDebug/GLObjectLog 와 동일한 수동 호출형 static 진단. GL·Effekseer 불의존.
    ///          모든 출력은 spdlog::warn/info — 빌드/실행을 막지 않는 진단 신호.
    class EffekseerDiagnostics
    {
    public:
        // 순수 static 유틸 — 인스턴스화 금지 (UniformDiagnostics 컨벤션과 동일).
        EffekseerDiagnostics()                                        = delete;
        EffekseerDiagnostics(const EffekseerDiagnostics &)            = delete;
        EffekseerDiagnostics &operator=(const EffekseerDiagnostics &) = delete;

        /// @brief .efk 가 참조하는 텍스처/모델/머티리얼이 base 기준 실제 존재하는지 검증.
        /// @param efkPath  .efk 파일 경로 (UTF-8). 슬래시 통일 권장.
        /// @param baseDir  텍스처 해석 기준 디렉토리. 비우면 efkPath 의 부모 디렉토리 사용
        ///                 (= Effekseer::Effect::Create 가 materialPath 없을 때 동작과 동일).
        /// @return 누락된 참조 수(0 = 전부 해석됨). 파일 없음 = -1, magic('SKFE') 불일치 = -2.
        /// @note 로드 전/후 무관 호출 가능 — Effekseer 비의존. 누락 ref 는 각각 spdlog::warn.
        static int CheckEffectTextures(std::string_view efkPath, std::string_view baseDir = {});

        /// @brief Manager::Play() 반환 핸들 검사 — Play 호출 직후.
        /// @param handle Play() 반환값 (Effekseer::Handle = int32_t). 음수 = 재생 실패.
        /// @param tag    로그 식별자(effect key 등). 비우면 생략.
        /// @return 유효(>=0) 시 true. 실패 시 spdlog::warn 후 false.
        static bool CheckPlayHandle(int32_t handle, std::string_view tag = {});

        /// @brief Play 직후 인스턴스가 즉시 사라졌는지 감지.
        /// @param handle Play() 반환 핸들.
        /// @param exists 호출자가 mManager->Exists(handle) 로 계산해 전달.
        /// @param tag    로그 식별자.
        /// @return handle>=0 && exists 면 true. handle 유효한데 !exists 면 warn 후 false.
        ///         handle<0 면 조용히 false(CheckPlayHandle 이 이미 보고).
        static bool CheckHandleAlive(int32_t handle, bool exists, std::string_view tag = {});
    };
}

#endif // __SJH_DIAGNOSTICS_EFFEKSEER_DIAGNOSTICS_H__
