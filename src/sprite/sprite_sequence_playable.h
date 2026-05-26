#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__

#include "playable/iplayable.h"            // SJH::Playable::IPlayable* 슬롯
#include "playable/playable_base.h"
#include "sprite_frame_clip.h"
#include "sprite_component.h"
#include <unordered_map>
#include <vector>

namespace SJH::SpriteSequence
{
    class SpriteSequencePlayable : public SJH::Playable::PlayableBase
    {
      public:
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);
        ~SpriteSequencePlayable() override;

        // === Multi-clip API (M4) ===
        /// @brief 클립 인덱스에 SpriteFrameClip* 등록. PlayClip 이 이를 선택.
        SpriteSequencePlayable& RegisterClip(int clipIdx, const SpriteFrameClip* clip);
        /// @brief 클립 진입 시 Play() 호출될 IPlayable* 등록 (여러 개 등록 가능).
        SpriteSequencePlayable& RegisterOnClipEnter(int clipIdx, SJH::Playable::IPlayable* sideEffect);
        /// @brief 지정 클립으로 전환 + elapsed_ 리셋 + 등록된 sideEffect Play.
        void PlayClip(int clipIdx);
        int  CurrentClip() const { return currentClipIdx_; }

      protected:
        void OnUpdate(float dt) override;

      private:
        SJH::Sprite::SpriteRenderer* sprite_;
        const SpriteFrameClip*        clip_;   // 하위 호환 기본 클립 (clipIdx=0 fallback)

        std::unordered_map<int, const SpriteFrameClip*>              clips_;
        std::unordered_map<int, std::vector<SJH::Playable::IPlayable*>> onClipEnter_;
        int currentClipIdx_ = 0;
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
