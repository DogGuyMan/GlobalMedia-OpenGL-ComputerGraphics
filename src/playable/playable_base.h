/**
 * @file playable_base.h
 * @brief @c SJH::Playable::PlayableBase - @c IPlayable + @c Component 다중 상속 abstract base.
 *
 * @details
 *  ### 책임
 *  - @c IPlayable 인터페이스와 @c SJH::Scene::Component 를 *다중 상속* 하는 abstract 중간 계층.
 *  - 공통 상태 (@c mPaused / @c mIsFinished / @c mElapsed / @c mIsLoop) 보관.
 *  - @c Play / @c Pause / @c Stop trivial 구현 + @c Update -> @c OnUpdate hook 디스패치.
 *  - @c SetIsLoop(bool) 세팅 단계 속성 제공 (spec sec.1 결정 #5-부속 - @c IPlayable 인터페이스 제외).
 *
 *  ### 비-책임
 *  - [X] 연출 로직 - concrete (@c SpriteSequencePlayable / @c SequencePlayable 등) 가 @c OnUpdate 에서 담당.
 *  - [X] Actor 부착 관리 - @c Actor::AddComponent 경로 외에도 Composite children 으로만 존재 가능.
 *  - [X] @c OnPause hook - 필요 시 concrete 가 @c Pause() 자체를 override.
 *
 * @note concrete 는 @c OnUpdate(float dt) *만 필수* override.
 *       @c OnPlay / @c OnStop 은 기본 empty (필요 시 override).
 *       vtable anchor: dtor out-of-line (@c playable_base.cpp 에 정의) - ODR 안정.
 */
#ifndef __SJH_PLAYABLE_PLAYABLE_BASE_H__
#define __SJH_PLAYABLE_PLAYABLE_BASE_H__

#include "playable/iplayable.h"
#include "scene/actor.h"   // SJH::Scene::Component

namespace SJH::Playable
{
    /**
     * @brief @c IPlayable + @c SJH::Scene::Component 다중 상속 abstract base.
     * @details
     *  공통 상태 (@c mPaused / @c mIsFinished / @c mElapsed / @c mIsLoop) + @c IPlayable
     *  4-method trivial 구현 + @c Component::Update -> @c OnUpdate hook 디스패치.
     *
     *  ### 상속 구조
     *  ```
     *  IPlayable          SJH::Scene::Component
     *      +-------------------+
     *             PlayableBase  (abstract)
     *                   |
     *       +-----------+---------------+
     *  SpriteSequence  SequencePlayable  ParallelPlayable  ...
     *  Playable
     *  ```
     *
     *  ### Update 흐름
     *  @c Actor::Update(dt) -> @c Component::Update(dt) ->
     *  @c PlayableBase::Update(dt) (@c final) -> @c OnUpdate(dt) hook.
     *  @c IsEnabled() || @c mPaused || @c mIsFinished 중 하나라도 true 이면 @c OnUpdate 진입 차단.
     *
     *  ### Composite children 특수 케이스 (spec sec.1 결정 #4)
     *  children 은 @c Actor 에 부착되지 않으므로 @c mOwner == nullptr - Composite (@c SequencePlayable /
     *  @c ParallelPlayable) 가 @c OnUpdate 내에서 children @c Update 를 *수동 디스패치*.
     */
    class PlayableBase : public IPlayable, public SJH::Scene::Component
    {
      protected:
        PlayableBase() = default;

      public:
        /// @brief out-of-line dtor - 다중 상속 vtable anchor (@c playable_base.cpp 에 정의, ODR 안정).
        ~PlayableBase() override;

        // === IPlayable 4-method + IsFinished ===

        /// @brief 재생 시작. @c mPaused / @c mIsFinished 를 @c false 로 리셋 후 @c OnPlay() hook 호출.
        void Play()  override
        {
            mPaused   = false;
            mIsFinished = false;
            OnPlay();
        }

        /// @brief 일시정지. @c mPaused = @c true. OnPause hook 없음 - 필요 시 concrete 가 @c Pause() override.
        void Pause() override { mPaused = true; }

        /// @brief 리셋 후 정지. @c mPaused / @c mIsFinished / @c mElapsed 초기화 후 @c OnStop() hook 호출.
        void Stop()  override
        {
            mPaused   = false;
            mIsFinished = false;
            mElapsed  = 0.0f;
            OnStop();
        }

        /// @brief 루프 여부 반환. @c mIsLoop 값. @c SetIsLoop(bool) 로 설정.
        bool GetIsLoop()  const override { return mIsLoop; }

        /// @brief 재생 종료 여부 반환. @c mIsFinished 값.
        bool IsFinished() const override { return mIsFinished; }

        // === Loop setter - 인터페이스 외 추가 (spec sec.1 결정 #5-부속) ===

        /// @brief 루프 여부 설정. 세팅 단계 속성 - @c IPlayable 인터페이스 외부. @c PlayableBase* 이상 핸들에서만 호출 가능.
        /// @param v @c true 이면 종료 시 자동 재시작, @c IsFinished() 절대 @c true 안 됨.
        void SetIsLoop(bool v) { mIsLoop = v; }

        // === Component 3 hook ===

        /// @brief Actor 씬 그래프 진입 시 호출. 기본 empty - 필요 시 concrete override.
        void OnEnter() override {}

        /// @brief Actor 씬 그래프 이탈 시 호출. 기본 empty - 필요 시 concrete override.
        void OnExit()  override {}

        /// @brief 매 프레임 호출 (@c final - override 금지). @c IsEnabled() / @c mPaused / @c mIsFinished 게이트 후
        ///        @c mElapsed 누적 + @c OnUpdate(dt) hook 디스패치.
        void Update(float dt) final
        {
            if (!IsEnabled() || mPaused || mIsFinished) return;
            mElapsed += dt;
            OnUpdate(dt);
        }

      protected:
        // === 파생 hook - concrete 가 override ===

        /// @brief @c Play() 직후 호출. 기본 empty. Composite 는 cursor 초기화 + 첫 child Play.
        virtual void OnPlay()   {}

        /// @brief @c Stop() 직후 호출. 기본 empty. Composite 는 모든 child @c Stop() 재귀 전파.
        virtual void OnStop()   {}

        /// @brief 매 프레임 연출 로직 - *유일 필수 override*. @c mElapsed 는 이미 누적된 상태로 진입.
        /// @param dt 프레임 델타 타임 (초).
        virtual void OnUpdate(float dt) = 0;

        // === 파생 공유 상태 (protected) ===

        /// @brief 일시정지 상태. @c Pause() = @c true, @c Play() = @c false.
        bool  mPaused   = false;

        /// @brief 재생 종료 상태. Loop=false 이고 연출 완료 시 @c true.
        bool  mIsFinished = false;

        /// @brief 루프 여부. @c SetIsLoop(bool) 로 설정.
        bool  mIsLoop   = false;

        /// @brief @c Play() 후 누적 경과 시간 (초). @c Stop() 호출 시 @c 0.0f 로 리셋.
        float mElapsed  = 0.0f;
    };
}

#endif // __SJH_PLAYABLE_PLAYABLE_BASE_H__
