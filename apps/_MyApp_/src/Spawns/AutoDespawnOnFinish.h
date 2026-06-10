/**
 * @file AutoDespawnOnFinish.h
 * @brief @c IPlayable 완료 감지 -> 다음 sweep 에서 자신의 Actor 를 자동 파괴하는 Component.
 *
 * @details
 *  ### 책임
 *  - 생성자에서 받은 @c IPlayable* (@p watched)의 @c IsFinished() 를 매 프레임 폴링.
 *  - 완료 감지 시 @c mDone = true 마킹 -- 실제 @c RemoveChild 는 부모가 @c Update 밖에서
 *    수행해 반복자 무효화를 회피한다 (Box2D @c b2World::Step 중 body 변경 금지와 동일 패턴).
 *  ### 비-책임
 *  - [X] Actor 트리에서의 실제 제거(RemoveChild) -- 부모의 @c SweepFinishedChildren 담당.
 *  - [X] @p watched Playable 의 재생/정지 -- 각 팩토리(@c SpawnAudioInstance 등)가 담당.
 *  ### 정통 매핑
 *  - Cocos2D @c Node::runAction(Sequence(..., RemoveSelf)) -- 동작 완료 후 자동 제거.
 *  - Unity @c Destroy(go, delay) / @c StartCoroutine 후 @c DestroyImmediate 패턴.
 *
 * @note @p watched 는 non-owning 포인터 -- 수명은 같은 Actor 의 다른 Component 가 소유.
 *       @c mWatched 가 nullptr 이면 @c mDone 은 영영 true 가 되지 않으므로 주의.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
#define __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__

#include "playable/iplayable.h"
#include "scene/actor.h"

namespace TopdownShooter::Spawns
{
    /**
     * @brief @c IPlayable 완료 감지 후 @c mDone = true 마킹 -- 부모 sweep 이 Actor 를 제거.
     * @details
     *  단발 스폰 프리미티브(@c SpawnAudioInstance / @c SpawnVfxInstance / @c SpawnWorldText)의
     *  공통 자동 파괴 seam. @p watched Playable 이 @c IsFinished() 를 반환하는 순간
     *  @c mDone = true 로 전환하고, 부모의 @c SweepFinishedChildren 가 @c Update 루프 밖에서
     *  @c RemoveChild 를 호출해 반복자 무효화를 안전하게 회피한다.
     */
    class AutoDespawnOnFinish : public SJH::Scene::Component
    {
      public:
        /// @brief 감시할 Playable 을 등록해 Component 를 생성.
        /// @param watched 완료 여부를 폴링할 @c IPlayable 포인터 (non-owning).
        ///                nullptr 이면 @c mDone 은 영영 true 가 되지 않는다.
        explicit AutoDespawnOnFinish(SJH::Playable::IPlayable* watched) : mWatched(watched) {}

        /// @brief 자동 파괴 마킹 완료 여부 조회.
        /// @return @p watched->IsFinished() 가 한 번이라도 true 였으면 true.
        bool IsDone() const { return mDone; }

        void OnEnter() override {}
        void OnExit()  override {}

        /// @brief 매 프레임 @p watched->IsFinished() 폴링 -- 완료 시 @c mDone = true.
        /// @param dt 프레임 델타 타임 (본 컴포넌트는 시간 무관, 시그니처 준수용).
        void Update(float /*dt*/) override
        {
            if (!mDone && mWatched && mWatched->IsFinished())
                mDone = true;
        }

      private:
        SJH::Playable::IPlayable* mWatched = nullptr; ///< 감시 대상 Playable (non-owning). 수명은 같은 Actor 의 다른 Component 가 소유.
        bool                      mDone    = false;    ///< true 면 부모 SweepFinishedChildren 가 다음 tick 에 Actor 제거.
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AUTO_DESPAWN_ON_FINISH_H__
