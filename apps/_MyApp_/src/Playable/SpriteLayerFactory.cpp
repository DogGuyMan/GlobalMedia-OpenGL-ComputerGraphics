/**
 * @file SpriteLayerFactory.cpp
 * @brief AttachSpriteLayer 구현 — atlas find-or-create + SpriteRenderer 부착 + 조건부 애니 기동.
 *
 * @details
 *  ### 구현 흐름
 *  1. @c reg.FindUniformAtlas(t.TexturePath) — 이미 로드된 atlas 재사용.
 *  2. 없으면 @c reg.CreateUniformAtlas(path, colCount, rowCount) — 신규 로드 + 캐시.
 *  3. nullptr 이면 @c spdlog::error 출력 후 nullptr 반환.
 *  4. @c target.AddComponent<SpriteRenderer>(atlas) — flipX / QueueOffset 적용.
 *  5. @c t.ColCount > 1 이면 @c SpriteSequencePlayable(loop=true).Play() 자동 기동.
 *
 * @note gl3w.h 를 최상단에 include 하는 이유: resource_registry.h -> framebuffer.h 경유
 *       간접 포함되기 전에 먼저 포함해야 GL 심볼 중복 정의를 피할 수 있다.
 */
#include <GL/gl3w.h> // 최상단 — resource_registry.h -> framebuffer.h -> ... -> gl3w.h 보다 먼저.

#include "Playable/SpriteLayerFactory.h"

#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_frame_clip.h"
#include "sprite/sprite_sequence_playable.h"

#include <spdlog/spdlog.h>

namespace TopdownShooter::Playable
{
	SJH::Sprite::SpriteRenderer *AttachSpriteLayer(
	    SJH::Scene::Actor &target, SJH::ResourceRegistry &reg,
	    const EntityTextureConfig &t, float fps)
	{
		// atlas 먼저 — key=path. 공유 PNG 중복키는 Find 재사용, 없으면 Create.
		auto *atlas = reg.FindUniformAtlas(t.TexturePath);
		if (!atlas)
			atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
		if (!atlas)
		{
			spdlog::error("[sprite] atlas load 실패: {}", t.TexturePath);
			return nullptr;
		}

		auto *spr        = target.AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
		spr->flipX       = t.Flip;
		spr->QueueOffset = t.DrawOrder; // painter 합성 층 (enemy=0 → 기본값과 동일, 무해)

		if (t.ColCount > 1) // 애니 파트 (가로 N프레임 스트립)
		{
			auto *seq = target.AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
			    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, fps});
			seq->SetIsLoop(true);
			seq->Play();
		}
		return spr;
	}
} // namespace TopdownShooter::Playable
