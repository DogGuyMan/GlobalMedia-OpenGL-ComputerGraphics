#include "sprite_sequence/sprite_sequence_playable.h"
#include "sprite_sequence/sprite_frame_clip.h"   // SJH::SpriteSequence::SpriteFrameClip (ctor 시그니처)
#include "sprite/sprite_component.h"             // SJH::Sprite::SpriteRenderer (ctor 시그니처)

namespace SJH::SpriteSequence
{
    SpriteSequencePlayable::SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                                    const SpriteFrameClip*       clip)
        : sprite_(spriteRef), clip_(clip)
    {
    }

    SpriteSequencePlayable::~SpriteSequencePlayable() = default;   // vtable anchor

    void SpriteSequencePlayable::OnUpdate(float /*dt*/)
    {
        if (!clip_ || !sprite_) return;
        // elapsed_ 는 PlayableBase 가 이미 += dt 처리 (Update final 안에서)
        if (clip_->fps <= 0.0f || clip_->frameCount <= 0) return;   // 잘못된 clip 가드

        const float frameDur = 1.0f / clip_->fps;
        int raw = static_cast<int>(elapsed_ / frameDur);

        if (isLoop_)
        {
            raw %= clip_->frameCount;                 // Phase 2 결정 — loop=true 자동 wrap
        }
        else if (raw >= clip_->frameCount)
        {
            raw = clip_->frameCount - 1;
            finished_ = true;                          // non-loop 자연 종료
        }
        sprite_->frameIdx = clip_->startFrame + raw;
    }
}
