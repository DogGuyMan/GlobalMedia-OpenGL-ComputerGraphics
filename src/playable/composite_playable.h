/**
 * @file composite_playable.h
 * @brief @c SJH::Playable::SequencePlayable / @c ParallelPlayable - Composite IPlayable 구현체.
 *
 * @details
 *  ### 책임
 *  - @c SequencePlayable: children 을 *순차* 재생. 현재 child 종료 시 다음 child @c Play().
 *  - @c ParallelPlayable: children 을 *동시* 재생. 전원 종료 시 자기 @c IsFinished = @c true.
 *  - DOTween / Tweeny 정통 fluent Builder - @c Append / @c Insert / @c AppendInterval / @c Join 이
 *    derived 타입 참조 반환(@c *this) -> 체이닝 시 derived 메서드 유지.
 *  - @c IPlayable 다형 중첩 무제한 (Sequence 안 Parallel 안 Sequence ...).
 *
 *  ### 비-책임
 *  - [X] children 을 @c Actor::AddComponent 로 등록하는 일 -
 *    children 은 @c mOwner=nullptr (@c Actor 에 미부착), Composite 가 @c OnUpdate 내에서 수동 디스패치.
 *  - [X] children 생성 - 호출자가 @c std::make_unique<T>(...) 로 생성 후 @c Append/@c Join 에 소유권 이전.
 *
 *  ### children Update 디스패치 패턴 (spec sec.1 결정 #4 trade-off)
 *  children 은 @c IPlayable* 로 보관되므로 @c SJH::Scene::Component* 로 @c dynamic_cast 후
 *  @c IsEnabled() 검사 -> @c Update(dt) 수동 호출.
 *  concrete child 가 @c PlayableBase 가 아닌 순수 @c IPlayable 구현체일 경우 cast 실패 -
 *  해당 child 는 Update 없이 상태만 조회됨 (현재 코드베이스 내 해당 사례 없음).
 *
 * @note 정본 spec: @c docs/superpowers/specs/2026-05-26-playable-component-interface-design.md sec.1 결정 #4/4-bis
 */
#ifndef __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
#define __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__

#include "playable/iplayable.h"          // SJH::Playable::IPlayable (children unique_ptr 원소 타입)
#include "playable/playable_base.h"      // PlayableBase 베이스
#include <cstddef>                        // std::size_t
#include <memory>                         // std::unique_ptr
#include <vector>                         // std::vector

namespace SJH::Playable
{
    /**
     * @brief 순차 Composite - children 을 한 번에 1개씩 순서대로 재생.
     * @details
     *  - 현재 child 가 @c IsFinished() 이면 cursor 를 다음으로 이동 + @c Play().
     *  - @c Loop=true 면 모든 children 종료 시 @c cursor=0 + 첫 child @c Play() - 무한 반복.
     *  - 빈 컨테이너 가드: children 이 없으면 @c OnUpdate 즉시 @c mIsFinished=true.
     *
     *  ### Fluent Builder (Tweeny `*this` + DOTween Append/Insert 정통)
     *  Builder 메서드가 @c SequencePlayable& 반환 - 체이닝 시 @c ParallelPlayable 등 derived 메서드 유실 없이 사용.
     *  ```cpp
     *  auto seq = std::make_unique<SequencePlayable>();
     *  seq->Append(std::make_unique<SpriteSequencePlayable>(...))
     *     .AppendInterval(0.5f)
     *     .Append(std::make_unique<FmodPlayable>(...));
     *  seq->SetIsLoop(true).Play();
     *  ```
     */
    class SequencePlayable : public PlayableBase
    {
      public:
        // === Fluent Builder (Tweeny `*this` + DOTween Append/Insert 정통) ===

        /// @brief children 맨 뒤에 @p child 추가. 소유권 이전 (@c unique_ptr move).
        /// @return @c *this - 연속 체이닝 가능.
        SequencePlayable& Append(std::unique_ptr<IPlayable> child);

        /// @brief @p pos 위치에 @p child 삽입. @p pos >= size() 이면 맨 뒤 삽입.
        /// @param pos 0-based 삽입 위치.
        /// @param child 소유권 이전할 playable.
        /// @return @c *this - 연속 체이닝 가능.
        SequencePlayable& Insert(std::size_t pos, std::unique_ptr<IPlayable> child);

        /// @brief @p seconds 초 대기 @c IntervalPlayable 을 맨 뒤에 추가.
        /// @param seconds 대기 시간 (초).
        /// @return @c *this - 연속 체이닝 가능.
        SequencePlayable& AppendInterval(float seconds);

        // === 디버그/관찰 ===

        /// @brief 등록된 children 수.
        std::size_t Size()   const { return mPlayableChildrens.size(); }

        /// @brief 현재 재생 중인 child 인덱스 (0-based). @c Size() 이상이면 종료 상태.
        std::size_t Cursor() const { return mPlayableCursor; }

      protected:
        /// @brief cursor=0, 첫 child @c Play().
        void OnPlay()   override;

        /// @brief 모든 children @c Stop() 후 cursor=0.
        void OnStop()   override;

        /// @brief 현재 cursor child Update -> IsFinished 확인 -> 다음 child 진행.
        void OnUpdate(float dt) override;

      private:
        /// @brief 순차 재생 대상 children (소유권 보유).
        std::vector<std::unique_ptr<IPlayable>> mPlayableChildrens;

        /// @brief 현재 재생 중인 child 인덱스.
        std::size_t mPlayableCursor = 0;
    };

    /**
     * @brief 병렬 Composite - children 을 모두 동시 재생.
     * @details
     *  - 모든 children 이 @c IsFinished() 이면 자기 @c mIsFinished = @c true.
     *  - @c Loop=true 면 모든 children 종료 시 전원 @c Play() 재시작 - 무한 반복.
     *  - 빈 컨테이너 가드: children 이 없으면 @c OnUpdate 즉시 @c mIsFinished=true.
     *
     *  ### Fluent Builder - @c Join
     *  ```cpp
     *  auto par = std::make_unique<ParallelPlayable>();
     *  par->Join(std::make_unique<SpriteSequencePlayable>(...))
     *     .Join(std::make_unique<FmodPlayable>(...));
     *  par->Play();
     *  ```
     *
     *  @note @c MEMORY.md [SequencePlayable+Effekseer 행] 패턴 주의:
     *        Sequence 안 Effekseer leaf 가 @c IsFinished() 를 @c true 로 안 만들면 후속 child 미도달.
     *        동시 연출은 Parallel 사용 권장.
     */
    class ParallelPlayable : public PlayableBase
    {
      public:
        /// @brief children 목록에 @p child 추가. 소유권 이전 (@c unique_ptr move).
        /// @return @c *this - 연속 체이닝 가능.
        ParallelPlayable& Join(std::unique_ptr<IPlayable> child);

        /// @brief 등록된 children 수.
        std::size_t Size() const { return mPlayableChildren.size(); }

      protected:
        /// @brief 모든 children @c Play().
        void OnPlay()   override;

        /// @brief 모든 children @c Stop().
        void OnStop()   override;

        /// @brief 미완료 children Update -> 전원 완료 시 Loop 재시작 또는 @c mIsFinished=true.
        void OnUpdate(float dt) override;

      private:
        /// @brief 병렬 재생 대상 children (소유권 보유).
        std::vector<std::unique_ptr<IPlayable>> mPlayableChildren;
    };
}

#endif // __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
