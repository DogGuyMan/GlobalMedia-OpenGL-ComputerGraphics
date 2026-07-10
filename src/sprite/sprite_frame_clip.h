/**
 * @file sprite_frame_clip.h
 * @brief SpriteSequencePlayable 에 주입되는 sprite 애니메이션 클립 데이터 POD.
 *
 * @details
 *  ### 책임
 *  - atlas 상의 시작 frame 인덱스(@c startFrame), 재생 frame 수(@c frameCount), 재생 속도(@c fps) 보관.
 *  - @c SpriteSequencePlayable 이 이 값을 읽어 매 Update 마다 frameIdx 를 진행.
 *
 *  ### 비-책임
 *  - [X] loop 여부 - @c PlayableBase::mIsLoop 가 흡수 (spec sec.1 결정 2; @c SpriteFrameClip 에 loop 필드 추가 금지).
 *  - [X] 재생 상태(elapsed, paused, finished) - @c PlayableBase 보유.
 *
 * @note spec 정본: @c doc/superpowers/specs/2026-05-26-playable-component-interface-design.md sec.1 결정 2, sec.5.
 */

#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__

namespace SJH::SpriteSequence
{
    /**
     * @brief sprite 애니메이션 클립 데이터 POD - atlas frame 시퀀스 정의.
     * @details
     *  loop 플래그 없음 - @c PlayableBase::mIsLoop 가 흡수 (spec sec.1 결정 2).
     *  @c SpriteSequencePlayable::RegisterClip 으로 여러 클립을 인덱스별로 등록 가능.
     */
    struct SpriteFrameClip
    {
        int   startFrame;   ///< atlas 의 시작 frame index (0-based, row-major)
        int   frameCount;   ///< 이 clip 의 frame 수 (1 이상)
        float fps;          ///< 초당 재생 frame 수 (양의 실수; 0 이하이면 재생 스킵)
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__
