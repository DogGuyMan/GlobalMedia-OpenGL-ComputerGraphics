#include "sprite_sequence_playable.h"
#include "playable/iplayable.h"
#include "sprite_frame_clip.h"
#include "sprite_component.h"

namespace SJH::SpriteSequence
{
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : sprite_(spriteRef), clip_(clip)
    {
    }

    SpriteSequencePlayable::~SpriteSequencePlayable() = default;

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterClip(int clipIdx,
                                                                   const SpriteFrameClip* clip)
    {
        clips_[clipIdx] = clip;
        return *this;
    }

    SpriteSequencePlayable& SpriteSequencePlayable::RegisterOnClipEnter(
        int clipIdx, SJH::Playable::IPlayable* sideEffect)
    {
        onClipEnter_[clipIdx].push_back(sideEffect);
        return *this;
    }

    void SpriteSequencePlayable::PlayClip(int clipIdx)
    {
        if (currentClipIdx_ == clipIdx) return;
        currentClipIdx_ = clipIdx;
        elapsed_        = 0.0f;
        finished_       = false;

        auto it = onClipEnter_.find(clipIdx);
        if (it != onClipEnter_.end())
        {
            for (auto* p : it->second)
                if (p) { p->Stop(); p->Play(); }
        }
    }

    void SpriteSequencePlayable::OnUpdate(float /*dt*/)
    {
        if (!sprite_) return;

        // 현재 클립 선택: clips_ 우선, fallback=clip_
        const SpriteFrameClip* clip = nullptr;
        auto it = clips_.find(currentClipIdx_);
        if (it != clips_.end())
            clip = it->second;
        else
            clip = clip_;

        if (!clip || clip->fps <= 0.0f || clip->frameCount <= 0) return;

        const float frameDur = 1.0f / clip->fps;
        int raw = static_cast<int>(elapsed_ / frameDur);

        if (isLoop_)
        {
            raw %= clip->frameCount;
        }
        else if (raw >= clip->frameCount)
        {
            raw       = clip->frameCount - 1;
            finished_ = true;
        }
        sprite_->frameIdx = clip->startFrame + raw;
    }
}
