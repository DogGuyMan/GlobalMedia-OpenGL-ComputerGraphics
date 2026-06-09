/**
 * @file composite_playable.cpp
 * @brief @c SequencePlayable / @c ParallelPlayable Composite 구현.
 *
 * @details
 *  ### 책임
 *  - @c SequencePlayable::Append / @c Insert / @c AppendInterval fluent Builder 정의.
 *  - @c SequencePlayable / @c ParallelPlayable 의 @c OnPlay / @c OnStop / @c OnUpdate hook 구현.
 *
 *  ### 비-책임
 *  - [X] children 생성 - 호출자 책임 (@c std::make_unique<T>(...) + @c Append/@c Join).
 *  - [X] children @c OnEnter / @c OnExit - Actor 미부착 children 은 생략 (수동 Update 만).
 *
 * @note children 은 @c IPlayable* 로 보관되므로 @c SJH::Scene::Component* 로 @c dynamic_cast 후
 *       @c IsEnabled() 검사 -> @c Update(dt) 수동 호출 (spec sec.1 결정 #4 trade-off).
 */
#include "playable/composite_playable.h"
#include "playable/interval_playable.h"   // AppendInterval 내부 생성
#include "playable/iplayable.h"           // SJH::Playable::IPlayable (Append/Insert/Join 시그니처)
#include "scene/actor.h"                  // SJH::Scene::Component (dynamic_cast 대상)
#include <cstddef>                        // std::size_t, std::ptrdiff_t
#include <memory>                         // std::unique_ptr
#include <utility>                        // std::move

namespace SJH::Playable
{
    // ----- SequencePlayable -----

    SequencePlayable& SequencePlayable::Append(std::unique_ptr<IPlayable> child)
    {
        mPlayableChildrens.push_back(std::move(child));
        return *this;
    }

    SequencePlayable& SequencePlayable::Insert(std::size_t pos, std::unique_ptr<IPlayable> child)
    {
        if (pos > mPlayableChildrens.size()) pos = mPlayableChildrens.size();
        // size_t -> ptrdiff_t 명시 캐스트 - .clangd `-Wconversion` strict 정책 회피
        mPlayableChildrens.insert(mPlayableChildrens.begin() + static_cast<std::ptrdiff_t>(pos), std::move(child));
        return *this;
    }

    SequencePlayable& SequencePlayable::AppendInterval(float seconds)
    {
        return Append(std::make_unique<IntervalPlayable>(seconds));
    }

    void SequencePlayable::OnPlay()
    {
        // cursor 리셋 + 첫 child 재생
        mPlayableCursor = 0;
        if (!mPlayableChildrens.empty()) mPlayableChildrens[0]->Play();
    }

    void SequencePlayable::OnStop()
    {
        // 재귀 stop 후 cursor 리셋 (spec sec.1 결정 #4 Stop 전파)
        for (auto& c : mPlayableChildrens) c->Stop();
        mPlayableCursor = 0;
    }

    void SequencePlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 - Loop=true 라도 무한 빈 루프 회피
        if (mPlayableChildrens.empty()) { mIsFinished = true; return; }

        // cursor 가 끝을 넘어선 경우: Loop 재시작 또는 종료
        if (mPlayableCursor >= mPlayableChildrens.size())
        {
            if (mIsLoop)
            {
                mPlayableCursor = 0;
                mPlayableChildrens[0]->Play();
            }
            else
            {
                mIsFinished = true;
            }
            return;
        }

        // children 은 Actor 미부착 -> Composite 가 명시 Update 디스패치.
        // IPlayable* 보유라 SJH::Scene::Component* 로 down-cast 필요 (spec sec.3.3 trade-off).
        if (auto* head = dynamic_cast<SJH::Scene::Component*>(mPlayableChildrens[mPlayableCursor].get()))
            if (head->IsEnabled()) head->Update(dt);

        // 현재 child 종료 시 다음 child 로 전진 + Play
        if (mPlayableChildrens[mPlayableCursor]->IsFinished())
        {
            ++mPlayableCursor;
            if (mPlayableCursor < mPlayableChildrens.size()) mPlayableChildrens[mPlayableCursor]->Play();
        }
    }

    // ----- ParallelPlayable -----

    ParallelPlayable& ParallelPlayable::Join(std::unique_ptr<IPlayable> child)
    {
        mPlayableChildren.push_back(std::move(child));
        return *this;
    }

    void ParallelPlayable::OnPlay()
    {
        // 모든 children 동시 재생
        for (auto& c : mPlayableChildren) c->Play();
    }

    void ParallelPlayable::OnStop()
    {
        // 모든 children 재귀 stop (spec sec.1 결정 #4 Stop 전파)
        for (auto& c : mPlayableChildren) c->Stop();
    }

    void ParallelPlayable::OnUpdate(float dt)
    {
        // 빈 컨테이너 가드 - vacuously true 로 mIsFinished 잘못 신호 회피
        if (mPlayableChildren.empty()) { mIsFinished = true; return; }

        bool allDone = true;
        for (auto& c : mPlayableChildren)
        {
            if (!c->IsFinished())
            {
                // 미완료 child: Component 다운캐스트 후 수동 Update
                if (auto* head = dynamic_cast<SJH::Scene::Component*>(c.get()))
                    if (head->IsEnabled()) head->Update(dt);
                if (!c->IsFinished()) allDone = false;
            }
        }
        // 전원 완료 시 Loop 재시작 또는 종료
        if (allDone)
        {
            if (mIsLoop)
            {
                for (auto& c : mPlayableChildren) c->Play();
            }
            else
            {
                mIsFinished = true;
            }
        }
    }
}
