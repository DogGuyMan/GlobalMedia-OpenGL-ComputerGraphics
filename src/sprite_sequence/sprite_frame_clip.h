#ifndef __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__
#define __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__

namespace SJH::SpriteSequence
{
    /// @brief atlas frame index 시퀀스 정의 POD.
    ///        loop 필드 없음 — PlayableBase.isLoop_ 가 흡수 (spec §1 결정 2, §5 폐기 항목).
    struct SpriteFrameClip
    {
        int   startFrame;        // atlas 의 시작 frame index
        int   frameCount;        // 이 clip 의 frame 수 (1 이상)
        float fps;               // 초당 frame (0 보다 큰 양수)
    };
}

#endif // __SJH_SPRITE_SEQUENCE_SPRITE_FRAME_CLIP_H__
