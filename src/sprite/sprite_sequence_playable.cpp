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

    // 값-소유 ctor — ownedClip_ 가 먼저 생성된 뒤 clip_ 가 그 주소를 가리킨다(선언 순서 ownedClip_ → clip_).
    // ownedClip_ 는 객체와 함께 안정 주소에 할당되고, 클래스는 move/copy 금지(IPlayable 가 = delete) +
    // AddComponent 의 make_unique in-place 생성이라 clip_ 댕글링 불가.
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    SpriteFrameClip              clip)
        : sprite_(spriteRef), ownedClip_(clip), clip_(&ownedClip_)
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
