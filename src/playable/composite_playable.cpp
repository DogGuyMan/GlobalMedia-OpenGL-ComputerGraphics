#include "playable/composite_playable.h"
#include "playable/iplayable.h"           // SJH::Playable::IPlayable (Append/Insert/Join 시그니처)
#include "scene/actor.h"                  // SJH::Scene::Component (dynamic_cast 대상)
#include <cstddef>                        // std::size_t, std::ptrdiff_t
#include <memory>                         // std::unique_ptr
#include <utility>                        // std::move

namespace SJH::Playable
{
    // ───── SequencePlayable ─────

    SequencePlayable& SequencePlayable::Append(std::unique_ptr<IPlayable> child)
    {
        children_.push_back(std::move(child));
        return *this;
    }

    SequencePlayable& SequencePlayable::Insert(std::size_t pos, std::unique_ptr<IPlayable> child)
    {
        if (pos > children_.size()) pos = children_.size();
        // size_t → ptrdiff_t 명시 캐스트 — .clangd `-Wconversion` strict 정책 회피
        children_.insert(children_.begin() + static_cast<std::ptrdiff_t>(pos), std::move(child));
        return *this;
    }

    void SequencePlayable::OnPlay()
    {
        cursor_ = 0;
        if (!children_.empty()) children_[0]->Play();
    }

    void SequencePlayable::OnStop()
    {
        for (auto& c : children_) c->Stop();
        cursor_ = 0;
    }

    void SequencePlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 — isLoop_=true 라도 무한 빈 루프 회피
        if (children_.empty()) { finished_ = true; return; }

        if (cursor_ >= children_.size())
        {
            if (isLoop_)
            {
                cursor_ = 0;
                children_[0]->Play();
            }
            else
            {
                finished_ = true;
            }
            return;
        }

        // children 은 Component 이지만 Actor 미부착 — Composite 가 명시 Update 디스패치.
        // IPlayable* 보유라 SJH::Scene::Component* 로 down-cast 필요 (spec §3.3 trade-off).
        if (auto* head = dynamic_cast<SJH::Scene::Component*>(children_[cursor_].get()))
            if (head->IsEnabled()) head->Update(dt);

        if (children_[cursor_]->IsFinished())
        {
            ++cursor_;
            if (cursor_ < children_.size()) children_[cursor_]->Play();
        }
    }

    // ───── ParallelPlayable ─────

    ParallelPlayable& ParallelPlayable::Join(std::unique_ptr<IPlayable> child)
    {
        children_.push_back(std::move(child));
        return *this;
    }

    void ParallelPlayable::OnPlay()
    {
        for (auto& c : children_) c->Play();
    }

    void ParallelPlayable::OnStop()
    {
        for (auto& c : children_) c->Stop();
    }

    void ParallelPlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 — vacuously true 로 finished_ 잘못 신호 회피
        if (children_.empty()) { finished_ = true; return; }

        bool allDone = true;
        for (auto& c : children_)
        {
            if (!c->IsFinished())
            {
                if (auto* head = dynamic_cast<SJH::Scene::Component*>(c.get()))
                    if (head->IsEnabled()) head->Update(dt);
                if (!c->IsFinished()) allDone = false;
            }
        }
        if (allDone)
        {
            if (isLoop_)
            {
                for (auto& c : children_) c->Play();
            }
            else
            {
                finished_ = true;
            }
        }
    }
}
