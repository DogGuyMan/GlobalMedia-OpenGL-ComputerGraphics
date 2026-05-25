#ifndef __SJH_PLAYABLE_IPLAYABLE_H__
#define __SJH_PLAYABLE_IPLAYABLE_H__

namespace SJH::Playable
{
    /// @brief 시간축 추상화 — Client 우선 4 메서드 (Play/Pause/Stop/GetIsLoop) + 2급 IsFinished.
    /// @details
    ///   - 저장소 `I*` 컨벤션 준수: pure interface, 모든 멤버 =0, protected ctor + delete copy/move.
    ///   - 실제 상태/디폴트 임플리먼테이션은 `PlayableBase` (다중 상속 abstract base) 가 흡수.
    ///   - Tick 메서드 없음 — Component 시스템의 `Update(float dt)` 가 그 역할 (PlayableBase 흡수).
    class IPlayable
    {
      protected:
        IPlayable() = default;

      public:
        virtual ~IPlayable() = default;
        IPlayable(const IPlayable&)            = delete;
        IPlayable& operator=(const IPlayable&) = delete;
        IPlayable(IPlayable&&)                 = delete;
        IPlayable& operator=(IPlayable&&)      = delete;

        // === Client 우선 4-method ===
        virtual void Play()  = 0;       // 재생 시작 / Pause 후 재개 / Stop 후 첫 프레임부터
        virtual void Pause() = 0;       // 일시정지 (상태 보존, Play 로 재개)
        virtual void Stop()  = 0;       // 리셋 후 정지 (재사용 대기, 다음 Play 는 첫 프레임)
        virtual bool GetIsLoop() const = 0;

        // === 2급 공개 — Composite 의 child 종료 감지 + 외부 despawn 결정 ===
        virtual bool IsFinished() const = 0;
    };
}

#endif // __SJH_PLAYABLE_IPLAYABLE_H__
