#include <GL/gl3w.h> // 반드시 최상단 — Manager.h→VFXSystem.h→EffekseerRendererGL.h(시스템 gl3.h)보다 먼저.

#include "Bootstrap/PlayerBuilder.h"

#include "Audio/AudioSystem.h"
#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Entity/Components/LifeComponents.h" // Components::Life — 사망 지연(SetDeathDelaySeconds) set
#include "Entity/Player/PlayerActor.h"
#include "Entity/Player/PlayerHand.h"
#include "Playable/Constants.h"        // TopdownShooter::Playable::FRONT_MOVE
#include "Playable/HpGrayscalePostFX.h"   // 체력 비율 → 화면 grayscale (상시 [A] 바인더)
#include "Playable/PlayableDirector.h"   // 연출 foundation — onFire/onDamage 이관 대상
#include "Playable/PostFXTweenPlayable.h" // hit PostFX 연출(비네팅)
#include "Playable/SpriteFxPlayable.h"    // hit-flash / dissolve 스프라이트 연출(Task6)
#include "InputHandler/ActorFolower.h"
#include "Manager.h"
#include "Physics/PhysicsLayer.h"
#include "VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/scene.h"
#include "sprite/sprite_component.h"
#include "sprite/sprite_sequence_playable.h"

#include <spdlog/spdlog.h>
#include <tweeny/tweeny.h>
#include <memory>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Bootstrap
{
	PlayerResult BuildPlayer(const PlayerDeps &deps)
	{
		auto &dir = SJH::Scene::Director::Get();

		PlayerResult result;

		TopdownShooter::Entity::Player::PlayerActorConfig pac;
		pac.name = "PlayerSprite";
		pac.life.hp = 100;
		pac.movement.speed = 3.0f;
		pac.controller.keyboard = deps.keyboard;
		pac.controller.mouse    = deps.mouse;
		pac.controller.camera   = deps.worldCamera; // 좌클릭 마우스→Ground raycast 용 (World 카메라)
		// 좌클릭(발사)/G키(피격) 연출은 PlayableDirector 로 이관됐다 — 액터 빌드 후
		// director.Register("fire"/"hit"/"death", ...) + 입력 콜백이 director.Play(key)/ReactDamaged (아래 블록).
		// (config 단계엔 director 가 아직 없으므로 콜백은 액터 빌드 뒤 SetFire/DamageCallback 로 주입.)
		pac.physics.world = deps.physicsWorld;
		pac.physics.size = vmath::vec2(1.0f, 1.0f);
		pac.physics.startPosition = vmath::vec2(0.0f, 0.0f);
		pac.physics.density = 1.0f;
		pac.physics.linearDamping = 5.0f;
		pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
		pac.physics.maskBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);
		// Weapon — 좌클릭 발사 시 bullet 을 spawn 할 물리 월드 + 데미지. (physics 분기에서 Weapon 부착)
		pac.weapon.damage = 10;
		pac.weapon.world  = deps.physicsWorld;

		// 단일방향 주입 제거 — 8그룹을 아래에서 직접 빌드(PlayerActor 의 if(direction!=nullptr) 단일블록 비활성).
		// pac.sprite.direction 기본값 nullptr 유지.
		// pac.sprite.fps 미설정 — SpriteCfg 기본값(PlayerActor.h SpriteCfg::fps) 이 단일 소스.

		auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
		spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
		spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

		// ─── PlayableDirector 연출 foundation ───────────────────────────────────
		// 게임 로직(HP/물리/입력)은 그대로 — 모든 연출/사운드는 director 의 named Playable 로.
		// 루트 액터에 1개 부착 = Life 의 IActorPresentation sink (Life::OnEnter 가 GetComponent 로 캐시).
		// AddChild(=Enter) 보다 먼저 부착해야 sink 가 해소된다.
		auto *director = spriteActor->AddComponent<TopdownShooter::Playable::PlayableDirector>();

		// ─── directional 8그룹(4방향×2포즈) child 빌드 + RegisterGroup (분해 Task6) ───
		{
			namespace P = TopdownShooter::Playable;
			using TopdownShooter::Entity::EFacing;
			using TopdownShooter::Entity::EPose;
			struct GrpSrc { EFacing f; EPose p; const std::vector<P::EntityTextureConfig> *layers; };
			const GrpSrc kGroups[] = {
			    {EFacing::Front, EPose::Idle, &P::PLAYER_FRONT_IDLE}, {EFacing::Back, EPose::Idle, &P::PLAYER_BACK_IDLE},
			    {EFacing::Left, EPose::Idle, &P::PLAYER_LEFT_IDLE},   {EFacing::Right, EPose::Idle, &P::PLAYER_RIGHT_IDLE},
			    {EFacing::Front, EPose::Move, &P::PLAYER_FRONT_MOVE}, {EFacing::Back, EPose::Move, &P::PLAYER_BACK_MOVE},
			    {EFacing::Left, EPose::Move, &P::PLAYER_LEFT_MOVE},   {EFacing::Right, EPose::Move, &P::PLAYER_RIGHT_MOVE},
			};
			auto           &reg  = SJH::ResourceRegistry::Get();
			constexpr float kFps = 8.0f; // 애니(ColCount>1) 초당 프레임 (SpriteCfg 기본과 동일)
			for (const auto &grp : kGroups)
			{
				P::PlayableDirector::DirGroup dg{};
				std::size_t                   li = 0;   // std::array 인덱스 — size_type 일치(-Wconversion 회피)
				for (const auto &t : *grp.layers)
				{
					auto *atlas = reg.FindUniformAtlas(t.TexturePath); // 공유 PNG 중복키 nullptr 회피
					if (!atlas)
						atlas = reg.CreateUniformAtlas(t.TexturePath, t.TexturePath, t.ColCount, t.RowCount);
					if (!atlas)
					{
						spdlog::error("[8layer] atlas 실패: {}", t.TexturePath);
						continue;
					}
					auto  child = std::make_unique<SJH::Scene::Actor>(
					    "player_dir_" + std::to_string(static_cast<int>(grp.f)) + "_" +
					    std::to_string(static_cast<int>(grp.p)) + "_L" + std::to_string(t.DrawOrder));
					auto *cp = spriteActor->AddChild(std::move(child));
					auto *spr = cp->AddComponent<SJH::Sprite::SpriteRenderer>(atlas);
					spr->flipX       = t.Flip;
					spr->QueueOffset = t.DrawOrder; // 초기 가시성 안 건드림 — RefreshDirectional 이 처리
					if (li < 4) dg.layers[li] = spr;
					if (t.ColCount > 1) // 걷기 애니(B) — t.ColCount 로만 판정
					{
						auto *seq = cp->AddComponent<SJH::SpriteSequence::SpriteSequencePlayable>(
						    spr, SJH::SpriteSequence::SpriteFrameClip{0, t.ColCount, kFps});
						seq->SetIsLoop(true);
						seq->Play(); // [A] scene-tick (child 소유, DirGroup 미보유)
					}
					++li;
				}
				director->RegisterGroup(grp.f, grp.p, dg);
			}
		}

		// 체력 비율 → 화면 grayscale ([A] 상시 바인더, director 무관). HP 닳을수록 무채색, HP0 시 완전 무채색.
		spriteActor->AddComponent<TopdownShooter::Playable::HpGrayscalePostFX>("grayscale_vignetting", "uGrayscaleAmount");

		// 사망 dissolve 가 보이도록 사망 후 1.5s 비활성 지연 — 그동안 director "death" Playable 이 dissolve 구동.
		// (지연 없으면 Life::DoDie 가 즉시 SetActive(false) → dissolve 무발현. SetDeathDelaySeconds 는 Life 공개 API.)
		if (auto *life = spriteActor->GetComponent<TopdownShooter::Entity::Components::Life>())
			life->SetDeathDelaySeconds(1.5f);

		// "fire" — 좌클릭: Sequence( Effekseer muzzle → Parallel( Fmod.shot ∥ FmodStudio.Slash ) ).
		// 자원은 startup 의 WarmupAudio(shot/Slash) + CreateEffect(muzzle) 에서 이미 로드 → build-time 해소.
		// 가드(shot&&muzzle&&slashEvt) 결과는 fire-time 해소와 동일 (자원은 1회 로드 후 안정).
		{
			auto &reg      = SJH::ResourceRegistry::Get();
			auto &audio    = TopdownShooter::Manager::Get().Audio();
			auto &vfx      = TopdownShooter::Manager::Get().VFX();
			auto *shot     = reg.FindSound("shot");
			auto *muzzle   = reg.FindEffect("muzzle");
			auto *slashEvt = audio.LoadEvent("event:/Slash");

			if (shot && muzzle && slashEvt)
			{
				auto seq = std::make_unique<SJH::Playable::SequencePlayable>();
				seq->Append(std::make_unique<TopdownShooter::VFX::EffekseerPlayable>(
				    vfx.GetManager(), muzzle, vmath::vec3(0.0f),
				    TopdownShooter::VFX::TrackPolicy::Static));

				auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
				par->Join(std::make_unique<TopdownShooter::Audio::FmodPlayable>(audio.GetSystem(), shot));
				par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(slashEvt));
				seq->Append(std::move(par));

				director->Register("fire", std::move(seq));
			}
		}

		// "hit" — 실제 피격(Life::DoDamaged→ReactDamaged→Play("hit")) 연출:
		//          Parallel( 화면 비네팅 플래시[PostFX] ∥ 스프라이트 hit-flash[Task6] ∥ Damaged 사운드 ).
		//          비네팅 = grayscale_vignetting.uVignetteAmount 0.45→0 (테두리 붉은 플래시);
		//          hit-flash = 플레이어 4-레이어 SpriteRenderer.enableHit 0.18s on (셰이더 uTime 애니).
		{
			auto &audio      = TopdownShooter::Manager::Get().Audio();
			auto *damagedEvt = audio.LoadEvent("event:/Damaged");

			auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
			par->Join(std::make_unique<TopdownShooter::Playable::PostFXTweenPlayable>(
			    "grayscale_vignetting", "uVignetteAmount",
			    tweeny::from(0.45f).to(0.0f).during(300).via(tweeny::easing::sinusoidalInOut)));
			par->Join(std::make_unique<TopdownShooter::Playable::SpriteHitFlashPlayable>(spriteActor.get()));
			if (damagedEvt)
				par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

			director->Register("hit", std::move(par));
		}

		// "death" — 사망(Life::DoDie→ReactDied→Play("death")) 연출: 스프라이트 dissolve(1.5s) 만.
		//           화면 grayscale 은 HpGrayscalePostFX 가 HP 비율로 상시 구동(사망=HP0 시 자동 완전 무채색)하므로
		//           death 컴포지트에서 제거(둘이 같은 uGrayscaleAmount 를 쓰면 충돌). 월드점 폭발은 [B] delegate.
		//           ⚠ 가시화 전제 = 위 SetDeathDelaySeconds(1.5s) (없으면 즉시 비활성 → dissolve 무발현).
		director->Register("death", std::make_unique<TopdownShooter::Playable::SpriteDissolvePlayable>(spriteActor.get(), 1.5f));

		// 입력 콜백 → director 경유 (PlayerController 는 여전히 audio/vfx 를 모름).
		// G키 = 피격 연출 테스트 — 실제 Life::DoDamaged 와 동일 경로(ReactDamaged→Play("hit"))로 흘려
		//        비네팅 플래시+Damaged 사운드를 즉시 확인 가능. (구 "damaged_test" 셰이크 stub 대체)
		if (auto *controller = spriteActor->GetComponent<Controller::PlayerController>())
		{
			controller->SetFireCallback([director] { director->Play("fire"); });
			controller->SetDamageCallback([director] { director->ReactDamaged(0); });
		}
		// ─────────────────────────────────────────────────────────────────────────

		// 4-레이어 바디는 CreatePlayerActor 가 child 로 생성 (spec §6.3).
		// PlayerResult.Sprite/SpriteSeq 는 애니(B) 레이어의 컴포넌트를 가리킨다 (호환용 — 없으면 nullptr).
		for (const auto &child : spriteActor->GetChildren())
		{
			if (auto *seq = child->GetComponent<SJH::SpriteSequence::SpriteSequencePlayable>())
			{
				result.Sprite    = child->GetComponent<SJH::Sprite::SpriteRenderer>();
				result.SpriteSeq = seq;
				break;
			}
		}

		result.SpriteActor = dir.Root().AddChild(std::move(spriteActor));

		// 손 — PlayerHands::OnEnter 가 자식 Hand actor 2개를 생성·부착 (player Y facing 상속 궤도).
		// SpriteActor 는 이미 entered → 부착 즉시 OnEnter 실행. (손 스프라이트 비주얼은 사용자 WIP)
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>();

		deps.worldCamera
		    ->GetOwner()
		    ->GetComponent<Controller::ActorFolower>()
		    ->SetFollowTarget(result.SpriteActor);

		// 초기 가시성 확정 — 32레이어가 active 로 enter(OnEnter/애니 시작)된 *뒤* Front/Idle 외 SetActive(false).
		director->RefreshDirectional();

		return result;
	}
}
