#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__

#include "playable/playable_base.h"           // PlayableBase 베이스
#include "sprite_frame_clip.h" // SpriteFrameClip
#include "sprite_component.h"          // SJH::Sprite::SpriteRenderer (frameIdx 갱신 대상)

namespace SJH::SpriteSequence
{
    /// @brief Sprite atlas frame 시퀀스 — PlayableBase.elapsed_ 흡수, OnUpdate 가 frame index 계산.
    /// @note  binding 주입: SpriteRenderer* + SpriteFrameClip*. 둘 다 외부 owner (Playable 은 raw ptr 만).
    class SpriteSequencePlayable : public SJH::Playable::PlayableBase
    {
      public:
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);
        ~SpriteSequencePlayable() override;   // out-of-line vtable anchor

      protected:
        void OnUpdate(float dt) override;

      private:
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
