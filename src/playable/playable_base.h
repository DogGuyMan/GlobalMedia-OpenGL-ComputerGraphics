#ifndef __SJH_PLAYABLE_PLAYABLE_BASE_H__
#define __SJH_PLAYABLE_PLAYABLE_BASE_H__

#include "playable/iplayable.h"
#include "scene/actor.h"   // SJH::Scene::Component

namespace SJH::Playable
{
    /// @brief IPlayable + Component 다중 상속 abstract base.
    ///        공통 상태 (paused_/finished_/elapsed_/isLoop_) + Play/Pause/Stop trivial 구현 + UpdateOnUpdate hook.
    /// @note  concrete 는 OnUpdate 만 필수 override. OnPlay / OnStop 은 default empty.
    class PlayableBase : public IPlayable, public SJH::Scene::Component
    {
      protected:
        PlayableBase() = default;

      public:
        ~PlayableBase() override;   // out-of-line — vtable anchor (.cpp 에 정의)

        // === IPlayable 4-method + IsFinished ===
        void Play()  override
        {
            mPaused   = false;
            mIsFinished = false;
            OnPlay();
        }
        void Pause() override { mPaused = true; }   // OnPause hook 없음 — concrete 가 Pause 자체 override 권장
        void Stop()  override
        {
            mPaused   = false;
            mIsFinished = false;
            mElapsed  = 0.0f;
            OnStop();
        }
        bool GetIsLoop()  const override { return mIsLoop; }
        bool IsFinished() const override { return mIsFinished; }

        // === Loop setter — 인터페이스 외 추가 (사용자 결정 5-부속) ===
        void SetIsLoop(bool v) { mIsLoop = v; }

        // === Component 3 hook ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) final
        {
            if (!IsEnabled() || mPaused || mIsFinished) return;
            mElapsed += dt;
            OnUpdate(dt);
        }

      protected:
        // === 파생 hook — concrete 가 override ===
        virtual void OnPlay()   {}
        virtual void OnStop()   {}
        virtual void OnUpdate(float dt) = 0;   // 유일 필수

        // === 파생 공유 상태 (protected) ===
        bool  mPaused   = false;
        bool  mIsFinished = false;
        bool  mIsLoop   = false;
        float mElapsed  = 0.0f;
    };
}

#endif // __SJH_PLAYABLE_PLAYABLE_BASE_H__
