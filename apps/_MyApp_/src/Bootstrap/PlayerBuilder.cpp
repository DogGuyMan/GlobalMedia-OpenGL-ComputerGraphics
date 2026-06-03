#include <GL/gl3w.h> // 반드시 최상단 — Manager.h->VFXSystem.h->EffekseerRendererGL.h(시스템 gl3.h)보다 먼저.

#include "Bootstrap/PlayerBuilder.h"

#include "Audio/AudioSystem.h"
#include "Audio/FmodPlayable.h"
#include "Audio/FmodStudioPlayable.h"
#include "Bootstrap/EntityPresentation.h"  // AttachEntityPresentation — Player/Enemy 공통 연출 클러스터
#include "Entity/Player/PlayerActor.h"
#include "Entity/Player/PlayerHand.h"
#include "Playable/Constants.h"        // TopdownShooter::Playable::FRONT_MOVE
#include "Playable/HpGrayscalePostFX.h"   // 체력 비율 -> 화면 grayscale (상시 [A] 바인더)
#include "Playable/PlayableDirector.h"   // RegisterGroup/DirGroup + "fire"/"hit" Register + RefreshDirectional
#include "Playable/PostFXTweenPlayable.h" // hit PostFX 연출(비네팅)
#include "Playable/SpriteFxPlayable.h"    // "hit" Parallel 의 SpriteHitFlashPlayable
#include "Playable/SpriteLayerFactory.h"  // AttachSpriteLayer — 3-빌더 공유 sprite-layer 부착 헬퍼
#include "InputHandler/ActorFolower.h"
#include "Manager.h"
#include "Physics/PhysicsLayer.h"
#include "VFX/EffekseerPlayable.h"
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/scene.h"

#include <spdlog/spdlog.h>
#include <tweeny/tweeny.h>
#include <memory>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Bootstrap
{
	namespace
	{
		/// @brief [분할] directional 8그룹(4방향×2포즈) child 빌드 + RegisterGroup.
		/// @details 각 그룹 = 4-레이어 child Actor(공유 헬퍼로 atlas+SpriteRenderer+애니) + DirGroup 핸들 등록.
		///          spriteActor 에 child 부착, director 에 그룹 등록. (가시성은 caller 의 RefreshDirectional 이 확정.)
		void BuildPlayerDirectionalGroups(SJH::Scene::Actor &spriteActor, Playable::PlayableDirector &director)
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
			auto &reg = SJH::ResourceRegistry::Get();
			for (const auto &grp : kGroups)
			{
				P::PlayableDirector::DirGroup dg{};
				std::size_t                   li = 0; // std::array 인덱스 — size_type 일치(-Wconversion 회피)
				for (const auto &t : *grp.layers)
				{
					// child(방향별 네이밍) 생성 후 공유 헬퍼로 atlas+SpriteRenderer(+애니) 부착.
					auto  child = std::make_unique<SJH::Scene::Actor>(
					    "player_dir_" + std::to_string(static_cast<int>(grp.f)) + "_" +
					    std::to_string(static_cast<int>(grp.p)) + "_L" + std::to_string(t.DrawOrder));
					auto *cp  = spriteActor.AddChild(std::move(child));
					auto *spr = P::AttachSpriteLayer(*cp, reg, t, P::PLAYER_ANIM_FPS); // QueueOffset/flipX/애니 일임
					if (!spr)
						continue; // atlas 실패 — li 미소비(빈 child 는 무해, 렌더 0)
					if (li < 4) dg.layers[li] = spr; // [A] scene-tick 은 헬퍼가 child 에 부착 (DirGroup 미보유)
					++li;
				}
				director.RegisterGroup(grp.f, grp.p, dg);
			}
		}

		/// @brief [분할] Player 전투 연출 등록 — "fire"(좌클릭) + "hit"(피격).
		/// @details "fire" = Sequence(Effekseer muzzle -> Parallel(Fmod.shot ∥ FmodStudio.Slash)). 자원 미확보 시 미등록(guard).
		///          "hit"  = Parallel(화면 비네팅 ∥ 스프라이트 hit-flash ∥ Damaged 사운드). 헬퍼 기본 "hit"(SpriteHitFlash)을
		///                   director 동일키 덮어쓰기(Register overwrite). 자원은 startup Warmup 에서 이미 로드 -> build-time 해소.
		void RegisterPlayerCombatPlayables(SJH::Scene::Actor &spriteActor, Playable::PlayableDirector &director)
		{
			// "fire" — 좌클릭: Sequence( Effekseer muzzle -> Parallel( Fmod.shot ∥ FmodStudio.Slash ) ).
			// 가드(shot&&muzzle&&slashEvt) 결과는 fire-time 해소와 동일 (자원은 1회 로드 후 안정).
			{
				auto &reg      = SJH::ResourceRegistry::Get();
				auto &audio    = TopdownShooter::Manager::Get().Audio();
				auto &vfx      = TopdownShooter::Manager::Get().VFX();
				auto *shot     = reg.FindSound("shoot"); //!
				auto *muzzle   = reg.FindEffect("muzzle"); //!
				auto *slashEvt = audio.LoadEvent("event:/Slash"); //!

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

					director.Register("fire", std::move(seq));
				}
			}

			// "hit" — 실제 피격(Life::DoDamaged->ReactDamaged->Play("hit")) 연출:
			//          Parallel( 화면 비네팅 플래시[PostFX] ∥ 스프라이트 hit-flash[Task6] ∥ Damaged 사운드 ).
			//          비네팅 = grayscale_vignetting.uVignetteAmount 0.45->0; hit-flash = SpriteRenderer.enableHit 0.18s.
			{
				auto &audio      = TopdownShooter::Manager::Get().Audio();
				auto *damagedEvt = audio.LoadEvent("event:/Damaged");

				auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
				par->Join(std::make_unique<TopdownShooter::Playable::PostFXTweenPlayable>(
				    "grayscale_vignetting", "uVignetteAmount",
				    tweeny::from(0.45f).to(0.0f).during(300).via(tweeny::easing::sinusoidalInOut)));
				par->Join(std::make_unique<TopdownShooter::Playable::SpriteHitFlashPlayable>(&spriteActor));
				if (damagedEvt)
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

				director.Register("hit", std::move(par));
			}
		}
	} // namespace

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
		pac.controller.camera   = deps.worldCamera; // 좌클릭 마우스->Ground raycast 용 (World 카메라)
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

		// ─── 공통 연출 클러스터 (Player/Enemy 공유 헬퍼) ─────────────────────────
		// director 부착(=Life 의 IActorPresentation sink, AddChild 전) + "death"=SpriteDissolve(1.5) +
		// SetDeathDelaySeconds(1.5) + 체력바(기본 녹색). "hit" 기본 SpriteHitFlash 는 아래 Parallel 로 overwrite.
		// 전부 pre-entry. 반환 director 로 이어서 8그룹 RegisterGroup + "fire"/"hit" 추가 등록.
		auto *director = AttachEntityPresentation(*spriteActor); // cfg 기본 = Player 값(dissolve/delay 1.5, 녹색)

		// directional 8그룹(4방향×2포즈) child 빌드 + RegisterGroup (분할 자유함수).
		BuildPlayerDirectionalGroups(*spriteActor, *director);

		// 체력 비율 -> 화면 grayscale ([A] 상시 바인더, director 무관). HP 닳을수록 무채색, HP0 시 완전 무채색.
		spriteActor->AddComponent<TopdownShooter::Playable::HpGrayscalePostFX>("grayscale_vignetting", "uGrayscaleAmount");

		// (사망 후 1.5s 비활성 지연 SetDeathDelaySeconds 는 위 AttachEntityPresentation 헬퍼가 처리.)

		// "fire"(좌클릭) + "hit"(피격) 전투 연출 등록 (분할 자유함수). "hit" 은 헬퍼 기본 SpriteHitFlash overwrite.
		RegisterPlayerCombatPlayables(*spriteActor, *director);

		// ("death"=SpriteDissolve(1.5s) 등록은 위 AttachEntityPresentation 헬퍼가 처리 — Enemy 와 공통.
		//  화면 grayscale 은 HpGrayscalePostFX 가 HP 비율로 상시 구동하므로 death 컴포지트에 미포함.
		//  월드점 폭발은 [B] delegate. 가시화 전제 = 헬퍼의 SetDeathDelaySeconds(1.5s).)

		// 입력 콜백 -> director 경유 (PlayerController 는 여전히 audio/vfx 를 모름).
		// G키 = 피격 연출 테스트 — 실제 Life::DoDamaged 와 동일 경로(ReactDamaged->Play("hit"))로 흘려
		//        비네팅 플래시+Damaged 사운드를 즉시 확인 가능. (구 "damaged_test" 셰이크 stub 대체)
		if (auto *controller = spriteActor->GetComponent<Controller::PlayerController>())
		{
			controller->SetFireCallback([director] { director->Play("fire"); });
			controller->SetDamageCallback([director] { director->ReactDamaged(0); });
		}
		// ─────────────────────────────────────────────────────────────────────────

		result.SpriteActor = dir.Root().AddChild(std::move(spriteActor));

		// 손 — PlayerHands::OnEnter 가 자식 Hand actor 2개를 생성·부착 (player Y facing 상속 궤도).
		// SpriteActor 는 이미 entered -> 부착 즉시 OnEnter 실행. (손 스프라이트 비주얼은 사용자 WIP)
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>();

		// (머리 위 체력바 AttachHealthBar 는 위 AttachEntityPresentation 헬퍼가 pre-entry 로 처리 — Enemy 와 공통.
		//  pre/post-entry 동작 동등: OnEnter 가 씬 진입 시 발화하므로 부착 시점 무관.)

		deps.worldCamera
		    ->GetOwner()
		    ->GetComponent<Controller::ActorFolower>()
		    ->SetFollowTarget(result.SpriteActor);

		// 초기 가시성 확정 — 32레이어가 active 로 enter(OnEnter/애니 시작)된 *뒤* Front/Idle 외 SetActive(false).
		director->RefreshDirectional();

		return result;
	}
}
