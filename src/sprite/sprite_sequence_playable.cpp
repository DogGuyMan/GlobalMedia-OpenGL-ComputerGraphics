#include "sprite_sequence_playable.h"
#include "playable/iplayable.h"
#include "sprite_frame_clip.h"
#include "sprite_component.h"

namespace SJH::SpriteSequence
{
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : mSpritePtr(spriteRef), mClipPtr(clip)
    {
    }

    // 값-소유 ctor — ownedClip_ 가 먼저 생성된 뒤 clip_ 가 그 주소를 가리킨다(선언 순서 ownedClip_ -> clip_).
    // ownedClip_ 는 객체와 함께 안정 주소에 할당되고, 클래스는 move/copy 금지(IPlayable 가 = delete) +
    // AddComponent 의 make_unique in-place 생성이라 clip_ 댕글링 불가.
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    SpriteFrameClip              clip)
        : mSpritePtr(spriteRef), mOwnedClip(clip), mClipPtr(&mOwnedClip)
    {
    }

    SpriteSequencePlayable::~SpriteSequencePlayable() = default;

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterClip(int clipIdx,
                                                                   const SpriteFrameClip* clip)
    {
        mClips[clipIdx] = clip;
        return *this;
    }

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterOnClipEnter(
        int clipIdx, SJH::Playable::IPlayable* sideEffect)
    {
        mOnClipEnter[clipIdx].push_back(sideEffect);
        return *this;
    }

    void SpriteSequencePlayable::PlayClip(int clipIdx)
    {
        if (mCurrentClipIdx == clipIdx) return;
        mCurrentClipIdx = clipIdx;
        mElapsed        = 0.0f;
        mIsFinished       = false;

        auto it = mOnClipEnter.find(clipIdx);
        if (it != mOnClipEnter.end())
        {
            for (auto* p : it->second)
                if (p) { p->Stop(); p->Play(); }
        }
    }

    void SpriteSequencePlayable::OnUpdate(float /*dt*/)
    {
        if (!mSpritePtr) return;

        // 현재 클립 선택: clips_ 우선, fallback=clip_
        const SpriteFrameClip* clip = nullptr;
        auto it = mClips.find(mCurrentClipIdx);
        if (it != mClips.end())
            clip = it->second;
        else
            clip = mClipPtr;

        if (!clip || clip->fps <= 0.0f || clip->frameCount <= 0) return;

        const float frameDur = 1.0f / clip->fps;
        int raw = static_cast<int>(mElapsed / frameDur);

        if (mIsLoop)
        {
            raw %= clip->frameCount;
        }
        else if (raw >= clip->frameCount)
        {
            raw       = clip->frameCount - 1;
            mIsFinished = true;
        }
        mSpritePtr->frameIdx = clip->startFrame + raw;
    }
}
