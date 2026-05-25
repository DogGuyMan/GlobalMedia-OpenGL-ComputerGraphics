#ifndef __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
#define __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__

#include "playable/playable_base.h"
#include <memory>
#include <vector>

namespace SJH::Playable
{
    /// @brief 순차 — children 한 번에 1개. 현재 child 가 IsFinished 면 다음 child Play.
    ///        Loop=true 면 모든 children 종료 시 cursor=0 + Play() 재시작.
    /// @note  Builder Append/Insert 가 `SequencePlayable&` 반환 (derived 타입) — 체이닝 시 derived 메서드 유지.
    class SequencePlayable : public PlayableBase
    {
      public:
        // === Fluent Builder (Tweeny `*this` + DOTween Append/Insert 정통) ===
        SequencePlayable& Append(std::unique_ptr<IPlayable> child);
        SequencePlayable& Insert(std::size_t pos, std::unique_ptr<IPlayable> child);

        // 디버그/관찰
        std::size_t Size()   const { return children_.size(); }
        std::size_t Cursor() const { return cursor_; }

      protected:
        void OnPlay()   override;
        void OnStop()   override;
        void OnUpdate(float dt) override;

      private:
        std::vector<std::unique_ptr<IPlayable>> children_;
        std::size_t cursor_ = 0;
    };

    /// @brief 병렬 — children 모두 동시 Play. 모두 IsFinished 일 때 자기 IsFinished.
    ///        Loop=true 면 모든 children 종료 시 자동 Play 재시작 (무한 반복).
    class ParallelPlayable : public PlayableBase
    {
      public:
        ParallelPlayable& Join(std::unique_ptr<IPlayable> child);

        std::size_t Size() const { return children_.size(); }

      protected:
        void OnPlay()   override;
        void OnStop()   override;
        void OnUpdate(float dt) override;

      private:
        std::vector<std::unique_ptr<IPlayable>> children_;
    };
}

#endif // __SJH_PLAYABLE_COMPOSITE_PLAYABLE_H__
