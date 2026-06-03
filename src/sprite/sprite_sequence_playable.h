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
        /// @brief 값-소유 ctor — clip 을 멤버(ownedClip_)에 복사 보관, clip_ 가 이를 가리킨다.
        ///        호출자가 외부 clip 저장소를 관리할 필요 없음 (비소유 포인터 footgun 제거).
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                SpriteFrameClip              clip);
        ~SpriteSequencePlayable() override;

        // === Multi-clip API (M4) ===
        /// @brief 클립 인덱스에 SpriteFrameClip* 등록. PlayClip 이 이를 선택.
        SpriteSequencePlayable& RegisterClip(int clipIdx, const SpriteFrameClip* clip);
        /// @brief 클립 진입 시 Play() 호출될 IPlayable* 등록 (여러 개 등록 가능).
        SpriteSequencePlayable& RegisterOnClipEnter(int clipIdx, SJH::Playable::IPlayable* sideEffect);
        /// @brief 지정 클립으로 전환 + elapsed_ 리셋 + 등록된 sideEffect Play.
        void PlayClip(int clipIdx);
        int  CurrentClip() const { return mCurrentClipIdx; }

      protected:
        void OnUpdate(float dt) override;

      private:
        SJH::Sprite::SpriteRenderer* mSpritePtr;
        SpriteFrameClip               mOwnedClip{}; // 값 ctor 사용 시 clip 값 보관 — clip_ 가 이를 가리킴
        const SpriteFrameClip*        mClipPtr;        // 하위 호환 기본 클립 (clipIdx=0 fallback)

        std::unordered_map<int, const SpriteFrameClip*>              mClips;
        std::unordered_map<int, std::vector<SJH::Playable::IPlayable*>> mOnClipEnter;
        int mCurrentClipIdx = 0;
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
