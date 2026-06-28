// SJH::Playable characterization - PlayableBase 상태머신 + Sequence/Parallel Composite 계약 잠금.
// 핵심: PlayableBase::Update(final) 가 mElapsed 누적 후 OnUpdate hook 디스패치.
//       Composite 는 children 을 Component* 로 dynamic_cast 후 수동 Update.
#include <catch2/catch_test_macros.hpp>

#include "playable/iplayable.h"
#include "playable/playable_base.h"
#include "playable/composite_playable.h"
#include "playable/interval_playable.h"

#include <memory>

using SJH::Playable::IPlayable;
using SJH::Playable::IntervalPlayable;
using SJH::Playable::ParallelPlayable;
using SJH::Playable::PlayableBase;
using SJH::Playable::SequencePlayable;

namespace
{
    // 지정 duration 경과 시 finished 되는 leaf mock.
    // mElapsed 는 PlayableBase::Update 가 자동 누적하므로 OnUpdate 에서 비교만.
    class FixedDurationPlayable : public PlayableBase
    {
      public:
        explicit FixedDurationPlayable(float duration) : mDuration(duration) {}

        // OnPlay/OnStop 카운트 - Play/Stop 전파 검증용.
        int playCount = 0;
        int stopCount = 0;
        int updateCount = 0;

        float Elapsed() const { return mElapsed; } // 테스트 관찰용 - protected mElapsed 접근 우회

      protected:
        void OnPlay() override { playCount++; }
        void OnStop() override { stopCount++; }
        void OnUpdate(float) override
        {
            updateCount++;
            if (mElapsed >= mDuration)
                mIsFinished = true;
        }

      private:
        float mDuration;
    };
}

TEST_CASE("Playable: PlayableBase Play 는 paused/finished 리셋 + OnPlay", "[playable]")
{
    FixedDurationPlayable p(1.0f);
    p.Pause();
    p.Play();
    REQUIRE(p.playCount == 1);
    // Play 직후 paused=false -> Update 가 진행됨
    p.Update(0.5f);
    REQUIRE(p.updateCount == 1);
    REQUIRE_FALSE(p.IsFinished());
}

TEST_CASE("Playable: Pause 는 Update 차단, Play 로 재개", "[playable]")
{
    FixedDurationPlayable p(2.0f);
    p.Update(0.5f); // elapsed 0.5
    p.Pause();
    p.Update(0.5f); // paused - OnUpdate 진입 차단
    REQUIRE(p.updateCount == 1);
    p.Play();       // 재개 (mElapsed 보존)
    p.Update(0.5f); // elapsed 1.0
    REQUIRE(p.updateCount == 2);
}

TEST_CASE("Playable: Stop 은 elapsed 리셋 + OnStop", "[playable]")
{
    FixedDurationPlayable p(2.0f);
    p.Update(1.0f);
    REQUIRE(p.Elapsed() > 0.0f);
    p.Stop();
    REQUIRE(p.stopCount == 1);
    REQUIRE(p.Elapsed() == 0.0f); // mElapsed 리셋
    REQUIRE_FALSE(p.IsFinished());
}

TEST_CASE("Playable: leaf 는 duration 경과 시 finished", "[playable]")
{
    FixedDurationPlayable p(1.0f);
    p.Update(0.5f);
    REQUIRE_FALSE(p.IsFinished());
    p.Update(0.5f); // elapsed 1.0 >= 1.0
    REQUIRE(p.IsFinished());
    // finished 후 Update 는 게이트로 차단됨
    int before = p.updateCount;
    p.Update(0.5f);
    REQUIRE(p.updateCount == before);
}

TEST_CASE("Playable: Sequence - A 종료 후 B 진행 (cursor)", "[playable]")
{
    auto seq = std::make_unique<SequencePlayable>();
    auto a = std::make_unique<FixedDurationPlayable>(1.0f);
    auto b = std::make_unique<FixedDurationPlayable>(1.0f);
    FixedDurationPlayable *aRaw = a.get();
    FixedDurationPlayable *bRaw = b.get();
    seq->Append(std::move(a)).Append(std::move(b));

    REQUIRE(seq->Size() == 2);
    seq->Play(); // OnPlay: cursor=0, A.Play
    REQUIRE(aRaw->playCount == 1);
    REQUIRE(seq->Cursor() == 0);

    seq->Update(1.0f); // A elapsed 1.0 finished -> cursor=1, B.Play
    REQUIRE(aRaw->IsFinished());
    REQUIRE(seq->Cursor() == 1);
    REQUIRE(bRaw->playCount == 1);
    REQUIRE_FALSE(seq->IsFinished()); // 아직 B 진행 중

    seq->Update(1.0f); // B elapsed 1.0 finished -> cursor=2
    REQUIRE(bRaw->IsFinished());
    REQUIRE(seq->Cursor() == 2);

    seq->Update(0.016f); // cursor>=size -> mIsFinished
    REQUIRE(seq->IsFinished());
}

TEST_CASE("Playable: Sequence - 빈 컨테이너는 즉시 finished", "[playable]")
{
    auto seq = std::make_unique<SequencePlayable>();
    seq->Play();
    seq->Update(0.016f);
    REQUIRE(seq->IsFinished());
}

TEST_CASE("Playable: Sequence Insert - 중간 삽입 반영", "[playable]")
{
    auto seq = std::make_unique<SequencePlayable>();
    seq->Append(std::make_unique<FixedDurationPlayable>(1.0f));
    seq->Append(std::make_unique<FixedDurationPlayable>(1.0f));
    seq->Insert(1, std::make_unique<FixedDurationPlayable>(1.0f)); // 중간 삽입
    REQUIRE(seq->Size() == 3);
    // pos > size 이면 맨 뒤 삽입
    seq->Insert(99, std::make_unique<FixedDurationPlayable>(1.0f));
    REQUIRE(seq->Size() == 4);
}

TEST_CASE("Playable: Sequence AppendInterval - IntervalPlayable 추가", "[playable]")
{
    auto seq = std::make_unique<SequencePlayable>();
    seq->AppendInterval(0.5f);
    REQUIRE(seq->Size() == 1);
    seq->Play();
    seq->Update(0.5f); // interval elapsed 0.5 -> finished -> cursor 전진
    seq->Update(0.016f); // cursor>=size -> seq finished
    REQUIRE(seq->IsFinished());
}

TEST_CASE("Playable: Parallel Join - 동시 진행, 전부 끝나야 finished", "[playable]")
{
    auto par = std::make_unique<ParallelPlayable>();
    auto a = std::make_unique<FixedDurationPlayable>(1.0f);
    auto b = std::make_unique<FixedDurationPlayable>(2.0f);
    FixedDurationPlayable *aRaw = a.get();
    FixedDurationPlayable *bRaw = b.get();
    par->Join(std::move(a)).Join(std::move(b));
    REQUIRE(par->Size() == 2);

    par->Play(); // 전원 Play
    REQUIRE(aRaw->playCount == 1);
    REQUIRE(bRaw->playCount == 1);

    par->Update(1.0f); // A finished, B elapsed 1.0 (not yet)
    REQUIRE(aRaw->IsFinished());
    REQUIRE_FALSE(bRaw->IsFinished());
    REQUIRE_FALSE(par->IsFinished()); // 전부 끝나지 않음

    par->Update(1.0f); // B elapsed 2.0 finished -> 전원 완료
    REQUIRE(bRaw->IsFinished());
    REQUIRE(par->IsFinished());
}

TEST_CASE("Playable: Parallel - 빈 컨테이너는 즉시 finished", "[playable]")
{
    auto par = std::make_unique<ParallelPlayable>();
    par->Play();
    par->Update(0.016f);
    REQUIRE(par->IsFinished());
}

TEST_CASE("Playable: Loop=true 면 Sequence 는 끝나도 finished 안 됨(자동 재시작)", "[playable]")
{
    auto seq = std::make_unique<SequencePlayable>();
    auto a = std::make_unique<FixedDurationPlayable>(1.0f);
    FixedDurationPlayable *aRaw = a.get();
    seq->Append(std::move(a));
    seq->SetIsLoop(true);
    seq->Play();

    seq->Update(1.0f);   // A finished -> cursor=1
    seq->Update(0.016f); // cursor>=size + Loop -> cursor=0, A.Play 재시작
    REQUIRE_FALSE(seq->IsFinished()); // Loop 이면 절대 finished 안 됨
    REQUIRE(aRaw->playCount == 2);    // 재시작으로 Play 2회
}

TEST_CASE("Playable: Loop=true 면 Parallel 도 finished 안 됨", "[playable]")
{
    auto par = std::make_unique<ParallelPlayable>();
    auto a = std::make_unique<FixedDurationPlayable>(1.0f);
    FixedDurationPlayable *aRaw = a.get();
    par->Join(std::move(a));
    par->SetIsLoop(true);
    par->Play();

    par->Update(1.0f); // A finished -> 전원 완료 + Loop -> 전원 재Play
    REQUIRE_FALSE(par->IsFinished());
    REQUIRE(aRaw->playCount == 2);
}
