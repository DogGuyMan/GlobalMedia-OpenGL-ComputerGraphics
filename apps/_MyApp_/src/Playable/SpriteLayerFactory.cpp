#include <GL/gl3w.h> // 최상단 — resource_registry.h → framebuffer.h → ... → gl3w.h 보다 먼저.

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
