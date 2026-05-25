#ifndef __SJH_SPRITE_SPRITE_ANIMATOR_H__
#define __SJH_SPRITE_SPRITE_ANIMATOR_H__

#include "scene/actor.h"        // SJH::Scene::Component
#include "sprite/uniform_atlas.h"

namespace SJH::Sprite
{
    /// @brief 시간 따라 UniformAtlas frame 자동 wrap-around (간단 Animator Component).
    /// @details
    ///   ### 동작
    ///   - Update(dt) 가 누적 시간 mElapsed += dt
    ///   - currentFrame = floor(mElapsed * fps) % FrameCount (loop) 또는 clamp (non-loop)
    ///   - default fps = atlas->FrameCount() — *1초에 atlas 전체 사이클*.
    ///     예: 4×4 atlas → 16fps, 2×2 atlas → 4fps. 사용자 명시 가능 (SetFps).
    ///
    ///   ### 좌하단 순회 보장
    ///   - Image::Load (stb_image) 가 V flip 적용 — GL UV(0,0) = PNG 좌하단
    ///   - ComputeUVRect: frameIdx=0 → 좌하단 tile, 이후 row-major (오른쪽 → 위)
    ///
    ///   ### 사용 예
    ///   @code
    ///   auto* anim = actor->AddComponent<SJH::Sprite::SpriteAnimator>();
    ///   anim->SetAtlas(&myAtlas);   // fps default = atlas->FrameCount()
    ///   // 매 프레임: anim->GetCurrentFrame() 으로 frameIdx query
    ///   @endcode
    ///
    ///   ### spec §1.6 SpriteSequence 와의 관계
    ///   spec §1.6 의 SpriteSequencePlayable + sprite_sequence 별도 모듈 패턴은
    ///   *복잡한 시퀀스 / Composite Playable* 용 정통. 본 SpriteAnimator 는 *단일 atlas
    ///   loop / clamp* 만 — 경량 사용처 (단일 캐릭터 idle/move 등) 용.
    class SpriteAnimator : public SJH::Scene::Component
    {
    public:
        SpriteAnimator() = default;

        /// @brief atlas 주입. SetUp 또는 첫 Update 전 호출 필수.
        SpriteAnimator& SetAtlas(UniformAtlas* atlas)
        {
            mAtlas = atlas;
            return *this;
        }

        /// @brief fps 명시. 0 또는 음수 = auto (atlas->FrameCount() = 1초 사이클).
        SpriteAnimator& SetFps(float fps)
        {
            mFps = fps;
            return *this;
        }

        /// @brief loop=true: 마지막 frame 후 0으로 wrap. false: 마지막 frame 도달 후 정지 + IsFinished()=true.
        SpriteAnimator& SetLoop(bool loop)
        {
            mLoop = loop;
            return *this;
        }

        /// @brief 시작 frame 명시 (default 0 = 좌하단). non-zero 면 mElapsed 도 그에 맞춰 보정 가능.
        SpriteAnimator& SetStartFrame(int startFrame)
        {
            mCurrentFrame = startFrame;
            return *this;
        }

        // === Component lifecycle ===
        void OnEnter() override {}
        void OnExit()  override {}
        void Update(float dt) override
        {
            if (!mAtlas || mFinished)
                return;

            const int total = mAtlas->FrameCount();
            if (total <= 0)
                return;

            mElapsed += dt;
            const float effectiveFps = (mFps > 0.0f) ? mFps : static_cast<float>(total);
            const int rawFrame = static_cast<int>(mElapsed * effectiveFps);

            if (mLoop)
            {
                mCurrentFrame = rawFrame % total;
            }
            else
            {
                if (rawFrame >= total)
                {
                    mCurrentFrame = total - 1;
                    mFinished = true;
                }
                else
                {
                    mCurrentFrame = rawFrame;
                }
            }
        }

        // === Accessors ===
        int            GetCurrentFrame() const { return mCurrentFrame; }
        float          GetElapsed()      const { return mElapsed; }
        bool           IsFinished()      const { return mFinished; }
        UniformAtlas*  GetAtlas()        const { return mAtlas; }
        float          GetFps()          const
        {
            if (mFps > 0.0f) return mFps;
            return mAtlas ? static_cast<float>(mAtlas->FrameCount()) : 0.0f;
        }

    private:
        UniformAtlas* mAtlas        = nullptr;
        float         mFps          = 0.0f;    // 0 = auto (FrameCount)
        bool          mLoop         = true;

        float         mElapsed      = 0.0f;
        int           mCurrentFrame = 0;
        bool          mFinished     = false;
    };
}

#endif // __SJH_SPRITE_SPRITE_ANIMATOR_H__
