/**
 * @file sprite_sequence_playable.h
 * @brief SpriteFrameClip 기반 sprite 애니메이션 Playable - PlayableBase 상속, SpriteRenderer.frameIdx 구동.
 *
 * @details
 *  ### 책임
 *  - @c SpriteFrameClip 시퀀스를 시간 기반으로 재생 -> @c SpriteRenderer::frameIdx 갱신.
 *  - 단일 클립(기본) 및 Multi-clip 인덱스 맵(@c RegisterClip) 지원.
 *  - 클립 전환 시 sideEffect @c IPlayable* 연쇄 실행(@c RegisterOnClipEnter).
 *  - 구 @c SpriteAnimator 의 상위 호환 대체 (M3.5, 2026-05-26).
 *
 *  ### 비-책임
 *  - [X] loop 필드 보유 - @c PlayableBase::mIsLoop 가 흡수.
 *  - [X] atlas UV 계산 - @c SpriteRenderer::Update 가 @c atlas->GetUVRect(frameIdx) 로 수행.
 *
 *  ### 사용 패턴
 *  @code
 *  // 단일 클립 (값-소유 ctor - 외부 clip 저장소 불필요)
 *  SJH::SpriteSequence::SpriteFrameClip clip{0, 8, 12.0f};
 *  auto* seq = actor->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(spr, clip);
 *  seq->SetIsLoop(true).Play();
 *
 *  // Multi-clip
 *  seq->RegisterClip(1, &walkClip).RegisterClip(2, &runClip);
 *  seq->RegisterOnClipEnter(2, sfxPlayable);  // clip 2 진입 시 sfx 재생
 *  seq->PlayClip(2);
 *  @endcode
 *
 * @note spec 정본: @c doc/superpowers/specs/2026-05-26-playable-component-interface-design.md sec.1.6.
 */

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
    /**
     * @brief SpriteFrameClip 을 시간 기반으로 재생해 SpriteRenderer.frameIdx 를 구동하는 Playable.
     * @details
     *  @c PlayableBase 를 상속 - @c Play / @c Pause / @c Stop / @c SetIsLoop / @c IsFinished 계약 준수.
     *  @c OnUpdate(dt) 에서 @c mElapsed 를 읽어 현재 frame 인덱스를 계산, @c SpriteRenderer::frameIdx 에 기록.
     *
     *  클립 선택 우선순위: @c mClips[mCurrentClipIdx] -> fallback @c mClipPtr (생성자 주입 클립).
     */
    class SpriteSequencePlayable : public SJH::Playable::PlayableBase
    {
      public:
        /// @brief 비소유 포인터 ctor - 외부 @c SpriteFrameClip 저장소 수명이 this 보다 길어야 함.
        /// @param spriteRef 구동할 @c SpriteRenderer 포인터 (non-owning).
        /// @param clip      재생할 클립 포인터 (non-owning, nullptr 금지).
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                const SpriteFrameClip*       clip);

        /// @brief 값-소유 ctor - @p clip 을 내부 @c mOwnedClip 에 복사, 외부 저장소 불필요.
        /// @details
        ///   @c mOwnedClip 이 this 와 함께 안정 주소에 할당 - @c AddComponent::make_unique in-place 생성이라
        ///   @c mClipPtr 댕글링 불가. move/copy 금지(@c IPlayable @c = delete) 로 이동 후 주소 변경 없음.
        /// @param spriteRef 구동할 @c SpriteRenderer 포인터 (non-owning).
        /// @param clip      복사할 @c SpriteFrameClip 값.
        SpriteSequencePlayable(SJH::Sprite::SpriteRenderer* spriteRef,
                                SpriteFrameClip              clip);

        ~SpriteSequencePlayable() override;

        // =====================================================================
        // Multi-clip API (M4)
        // =====================================================================

        /// @brief @p clipIdx 에 @c SpriteFrameClip* 등록. @c PlayClip(clipIdx) 가 이를 선택.
        /// @param clipIdx 클립 인덱스 (0 이상, 사용자 정의).
        /// @param clip    등록할 클립 포인터 (non-owning, @c nullptr 이면 fallback @c mClipPtr 사용).
        /// @return @c *this (체이닝용).
        SpriteSequencePlayable& RegisterClip(int clipIdx, const SpriteFrameClip* clip);

        /// @brief @p clipIdx 진입 시 @c Play() 가 호출될 @c IPlayable* 를 등록 (여러 개 등록 가능).
        /// @param clipIdx    트리거 클립 인덱스.
        /// @param sideEffect 클립 진입 시 @c Stop() -> @c Play() 순으로 구동될 IPlayable (non-owning).
        /// @return @c *this (체이닝용).
        SpriteSequencePlayable& RegisterOnClipEnter(int clipIdx, SJH::Playable::IPlayable* sideEffect);

        /// @brief 지정 클립으로 전환 - @c mElapsed 리셋 + @c mIsFinished=false + sideEffect 실행.
        /// @details 이미 같은 클립이면 no-op.
        /// @param clipIdx 전환할 클립 인덱스.
        void PlayClip(int clipIdx);

        /// @brief 현재 재생 중인 클립 인덱스.
        int  CurrentClip() const { return mCurrentClipIdx; }

      protected:
        /// @brief PlayableBase hook - mElapsed 기반 frame 인덱스 계산 + SpriteRenderer 갱신.
        /// @param dt 프레임 델타 타임 (초). PlayableBase::Update 가 mElapsed += dt 후 호출.
        void OnUpdate(float dt) override;

      private:
        SJH::Sprite::SpriteRenderer* mSpritePtr;          ///< 구동 대상 SpriteRenderer (non-owning)
        SpriteFrameClip               mOwnedClip{};        ///< 값-소유 ctor 사용 시 클립 복사본
        const SpriteFrameClip*        mClipPtr;            ///< 기본 클립 포인터 (clipIdx fallback)

        std::unordered_map<int, const SpriteFrameClip*>              mClips;        ///< clipIdx -> 클립 포인터 맵
        std::unordered_map<int, std::vector<SJH::Playable::IPlayable*>> mOnClipEnter; ///< clipIdx -> sideEffect 목록
        int mCurrentClipIdx = 0;                           ///< 현재 활성 클립 인덱스
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_SEQUENCE_PLAYABLE_H__
