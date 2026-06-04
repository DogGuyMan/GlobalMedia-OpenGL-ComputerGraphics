#include <GL/gl3w.h> // 반드시 최상단 — Manager.h->VFXSystem.h->EffekseerRendererGL.h(시스템 gl3.h)보다 먼저.

#include "Bootstrap/PlayerBuilder.h"

#include "Audio/AudioSystem.h"
#include "Audio/Constants.h"          // EVENT_SLASH / EVENT_DAMAGED / EVENT_SHOOT
#include "Audio/FmodStudioPlayable.h"
#include "Bootstrap/EntityPresentation.h"  // AttachEntityPresentation — Player/Enemy 공통 연출 클러스터
#include "Entity/Components/LifeComponents.h"   // GetComponent<Life> (SetOnHitFx seam 주입)
#include "Entity/Components/WeaponComponents.h" // GetComponent<Weapon> (SetOnFireFx seam 주입)
#include "Entity/Player/PlayerActor.h"
#include "Entity/Player/PlayerEntity.h"         // GetComponent<PlayerEntity> (SetOnMoveFx seam 주입)
#include "Entity/Player/PlayerHand.h"
#include "Bootstrap/Constants.h"  // PLAYER_DECAL_Y / DECAL_CIRCLE_Y_DELTA
#include "Playable/Constants.h"        // TopdownShooter::Playable::FRONT_MOVE
#include "Playable/HpGrayscalePostFX.h"   // 체력 비율 -> 화면 grayscale (상시 [A] 바인더)
#include "Playable/PlayableDirector.h"   // RegisterGroup/DirGroup + "fire"/"hit" Register + RefreshDirectional
#include "Playable/PostFXTweenPlayable.h" // hit PostFX 연출(비네팅)
#include "Playable/SpriteFxPlayable.h"    // "hit" Parallel 의 SpriteHitFlashPlayable
#include "Playable/SpriteLayerFactory.h"  // AttachSpriteLayer — 3-빌더 공유 sprite-layer 부착 헬퍼
#include "InputHandler/ActorFolower.h"
#include "Manager.h"
#include "Physics/PhysicsLayer.h"
#include "Spawns/VfxInstance.h"   // VFX::Spawn 파사드 (seam 주입 람다 본문)
#include "Spawns/WorldTextInstance.h"   // WorldText::SpawnDamage 파사드 (데미지 숫자 seam 주입)
#include "playable/composite_playable.h"
#include "resource_registry/resource_registry.h"
#include "object/mesh.h"              // SJH::Mesh::CreatePlane
#include "render/mesh_renderer.h"     // SJH::Scene::MeshRenderer
#include "material/material.h"        // SJH::Material
#include "material/pass.h"            // SJH::Pass::Kind::Transparent
#include "resource_registry/image.h"  // SJH::Image::Load
#include "Physics/PhysicsComponent.h" // FindPhysics / Physics::GetBody
#include "scene/actor.h"
#include "scene/scene.h"

#include <box2d/box2d.h> // b2Shape / b2PolygonShape / b2Fixture
#include <spdlog/spdlog.h>
#include <algorithm> // std::max
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

		/// @brief [분할] Player 전투 연출 등록 — "fire"(좌클릭) + "dash"(Shift) + "hit"(피격).
		/// @details "fire" = FmodStudio.Shoot 단독(발사음). "dash" = FmodStudio.Dash 단독(대시음, PlayerEntity::Dash rising-edge 발동).
		///          "hit"  = Parallel(화면 비네팅 ∥ 스프라이트 hit-flash ∥ Damaged 사운드). 헬퍼 기본 "hit"(SpriteHitFlash)을
		///                   director 동일키 덮어쓰기(Register overwrite). 자원은 startup Warmup 에서 이미 로드 -> build-time 해소.
		void RegisterPlayerCombatPlayables(SJH::Scene::Actor &spriteActor, Playable::PlayableDirector &director)
		{
			// "fire" — 좌클릭: 발사음(event:/Shoot)만 재생. muzzle VFX 는 weapon onFireFx 의 "gunshoot" 가 별도 담당.
			{
				auto &audio    = TopdownShooter::Manager::Get().Audio();
				auto *shootEvt = audio.LoadEvent(Audio::EVENT_SHOOT); // 발사 SFX (Studio 이벤트)
				if (shootEvt)
					director.Register("fire", std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(shootEvt));
				else
					spdlog::warn("[BuildPlayer] event:/Shoot 미로드 — 발사음 없음 (bank 이벤트/재export 확인)");
			}

			// "dash" — Shift 대시: 대시음(event:/Dash)만 재생. PlayerEntity::Dash 가 실제 발동(rising edge)에서 Play("dash").
			{
				auto &audio   = TopdownShooter::Manager::Get().Audio();
				auto *dashEvt = audio.LoadEvent(Audio::EVENT_DASH); // 대시 SFX (Studio 이벤트)
				if (dashEvt)
					director.Register("dash", std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(dashEvt));
				else
					spdlog::warn("[BuildPlayer] event:/Dash 미로드 — 대시음 없음 (bank 이벤트/재export 확인)");
			}

			// "hit" — 실제 피격(Life::DoDamaged->ReactDamaged->Play("hit")) 연출:
			//          Parallel( 화면 비네팅 플래시[PostFX] ∥ 스프라이트 hit-flash[Task6] ∥ Damaged 사운드 ).
			//          비네팅 = grayscale_vignetting.uVignetteAmount 0.45->0; hit-flash = SpriteRenderer.enableHit 0.18s.
			{
				auto &audio      = TopdownShooter::Manager::Get().Audio();
				auto *damagedEvt = audio.LoadEvent(Audio::EVENT_DAMAGED);

				auto par = std::make_unique<SJH::Playable::ParallelPlayable>();
				par->Join(std::make_unique<TopdownShooter::Playable::PostFXTweenPlayable>(
				    "grayscale_vignetting", "uVignetteAmount",
				    tweeny::from(TopdownShooter::Playable::VIGNETTE_PEAK).to(0.0f).during(TopdownShooter::Playable::VIGNETTE_DURATION_MS).via(tweeny::easing::sinusoidalInOut)));
				par->Join(std::make_unique<TopdownShooter::Playable::SpriteHitFlashPlayable>(&spriteActor));
				if (damagedEvt)
					par->Join(std::make_unique<TopdownShooter::Audio::FmodStudioPlayable>(damagedEvt));

				director.Register("hit", std::move(par));
			}
		}
		/// @brief [데칼] 엔티티 발밑 그림자 + 피격범위 원 — groundActor 자식 1개 아래 MeshRenderer 2장.
		/// @details depth+1: root.groundActor.{decal_shadow, decal_hitrange}. 워블/스케일과 독립 Transform.
		///          공유 자원은 find-or-create (스폰마다 호출돼도 1회 생성). 크기는 첫 fixture 반경 자동.
		void AttachGroundDecals(SJH::Scene::Actor &root, float baseY)
		{
			auto &reg = SJH::ResourceRegistry::Get();

			// ── 공유 자원 (find-or-create) ──
			SJH::Program *prog = reg.FindProgram("simple_texture");
			if (!prog)
				prog = reg.CreateProgram("simple_texture",
				    "./resources/shaders/simple_texture.vs",
				    "./resources/shaders/simple_texture.fs");
			if (!prog)
			{
				spdlog::error("AttachGroundDecals: simple_texture program 생성 실패 — 데칼 생략");
				return;
			}

			SJH::Mesh *plane = reg.FindMesh("_ground_plane");
			if (!plane)
				plane = reg.RegisterMesh("_ground_plane", SJH::Mesh::CreatePlane());
			if (!plane)
			{
				spdlog::error("AttachGroundDecals: _ground_plane 메시 생성 실패 — 데칼 생략");
				return;
			}

			SJH::Texture *shadowTex = reg.FindTexture("entity_shadow");
			if (!shadowTex)
			{
				auto img = SJH::Image::Load("entity_shadow", "resources/texture/EntityShadow.png");
				if (img)
					shadowTex = reg.CreateTexture("entity_shadow", img.get());
			}

			SJH::Texture *circleTex = reg.FindTexture("hit_range_circle");
			if (!circleTex)
			{
				auto img = SJH::Image::Load("hit_range_circle", "resources/texture/Circle_albedo.png");
				if (img)
					circleTex = reg.CreateTexture("hit_range_circle", img.get());
			}
			if (!shadowTex || !circleTex)
			{
				spdlog::error("AttachGroundDecals: 데칼 텍스처 로드 실패 — 데칼 생략");
				return;
			}

			SJH::Material *shadowMat = reg.FindSharedMaterial("shadow_decal_mat");
			if (!shadowMat)
			{
				shadowMat = reg.CreateSharedMaterial("shadow_decal_mat");
				if (!shadowMat)
				{
					spdlog::error("AttachGroundDecals: shadow_decal_mat 생성 실패 — 데칼 생략");
					return;
				}
				shadowMat->SetProgram(prog);
				shadowMat->SetPass(SJH::Pass::Kind::Transparent);
				shadowMat->Properties.Textures["uTex"]   = {shadowTex, 0};
				shadowMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 1.0f, 1.0f, 0.5f);
			}
			SJH::Material *hitMat = reg.FindSharedMaterial("hitrange_decal_mat");
			if (!hitMat)
			{
				hitMat = reg.CreateSharedMaterial("hitrange_decal_mat");
				if (!hitMat)
				{
					spdlog::error("AttachGroundDecals: hitrange_decal_mat 생성 실패 — 데칼 생략");
					return;
				}
				hitMat->SetProgram(prog);
				hitMat->SetPass(SJH::Pass::Kind::Transparent);
				hitMat->Properties.Textures["uTex"]   = {circleTex, 0};
				hitMat->Properties.Vec4s["baseColor"] = vmath::vec4(1.0f, 0.0f, 0.0f, 0.45f);
			}

			// ── 충돌 반경 (첫 fixture) ──
			float hitRadius = 0.5f;
			if (auto *phys = TopdownShooter::Physics::Components::FindPhysics(&root))
			{
				if (b2Body *body = phys->GetBody())
				{
					if (b2Fixture *fx = body->GetFixtureList())
					{
						const b2Shape *sh = fx->GetShape();
						if (sh->GetType() == b2Shape::e_circle)
							hitRadius = sh->m_radius;
						else if (sh->GetType() == b2Shape::e_polygon)
						{
							const auto *poly   = static_cast<const b2PolygonShape *>(sh);
							float        maxExt = 0.0f;
							for (int32 i = 0; i < poly->m_count; ++i)
								maxExt = std::max(maxExt, poly->m_vertices[i].Length());
							hitRadius = maxExt;
						}
					}
				}
			}
			const float hitD    = hitRadius * 2.0f;
			const float shadowD = hitRadius * 2.0f * 1.2f;

			// ── groundActor + 데칼 2장 ──
			auto *ground = root.AddChild(std::make_unique<SJH::Scene::Actor>("groundActor"));

			auto *shadow = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_shadow"));
			shadow->AddComponent<SJH::Scene::MeshRenderer>(plane, shadowMat, /*queueOffset*/ 0);
			shadow->GetTransform().EulerRot[0] = -90.0f;                        // XY → XZ 눕힘
			shadow->GetTransform().Scale       = vmath::vec3(shadowD, 1.0f, shadowD);
			shadow->GetTransform().Translate   = vmath::vec3(0.0f, baseY, 0.0f);                        // z-fight 회피

			auto *circle = ground->AddChild(std::make_unique<SJH::Scene::Actor>("decal_hitrange"));
			circle->AddComponent<SJH::Scene::MeshRenderer>(plane, hitMat, /*queueOffset*/ 1);
			circle->GetTransform().EulerRot[0] = -90.0f;
			circle->GetTransform().Scale       = vmath::vec3(hitD, 1.0f, hitD);
			circle->GetTransform().Translate   = vmath::vec3(0.0f, baseY + DECAL_CIRCLE_Y_DELTA, 0.0f);
		}
	} // namespace

	PlayerResult BuildPlayer(const PlayerDeps &deps)
	{
		auto &dir = SJH::Scene::Director::Get();

		PlayerResult result;

		TopdownShooter::Entity::Player::PlayerActorConfig pac;
		pac.name = "PlayerSprite";
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
		pac.physics.categoryBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PhysicsLayer::Player);
		pac.physics.maskBits = TopdownShooter::Physics::ToBits(TopdownShooter::Physics::PlayerMask);
		// Weapon — 좌클릭 발사 시 bullet 을 spawn 할 물리 월드 + 데미지. (physics 분기에서 Weapon 부착)
		pac.weapon.world  = deps.physicsWorld;

		// 단일방향 주입 제거 — 8그룹을 아래에서 직접 빌드(PlayerActor 의 if(direction!=nullptr) 단일블록 비활성).
		// pac.sprite.direction 기본값 nullptr 유지.
		// pac.sprite.fps 미설정 — SpriteCfg 기본값(PlayerActor.h SpriteCfg::fps) 이 단일 소스.

		auto spriteActor = TopdownShooter::Entity::Player::CreatePlayerActor(pac);
		spriteActor->GetTransform().Translate = vmath::vec3(0.0f, 0.0f, 0.0f);
		spriteActor->GetTransform().Scale = vmath::vec3(1.0f, 1.0f, 1.0f);

		// depth+1: renderActor(방향 스프라이트) + aimPivot(조준 회전·손 궤도) — root는 비회전(데칼 spin 분리).
		// 둘 다 std::move 후에도 주소 안정(spriteActor children 보유). groundActor 는 AttachGroundDecals 가 root 직속에 부착.
		auto *renderActor = spriteActor->AddChild(std::make_unique<SJH::Scene::Actor>("renderActor"));
		auto *aimPivot    = spriteActor->AddChild(std::make_unique<SJH::Scene::Actor>("aimPivot"));

		// ─── 공통 연출 클러스터 (Player/Enemy 공유 헬퍼) ─────────────────────────
		// director 부착(=Life 의 IActorPresentation sink, AddChild 전) + "death"=SpriteDissolve(1.5) +
		// SetDeathDelaySeconds(1.5) + 체력바(기본 녹색). "hit" 기본 SpriteHitFlash 는 아래 Parallel 로 overwrite.
		// 전부 pre-entry. 반환 director 로 이어서 8그룹 RegisterGroup + "fire"/"hit" 추가 등록.
		auto *director = AttachEntityPresentation(*spriteActor); // cfg 기본 = Player 값(dissolve/delay 1.5, 녹색)

		// directional 8그룹(4방향×2포즈) child 빌드 + RegisterGroup (분할 자유함수).
		// renderActor 하위에 부착 — root 비회전이라도 빌보드 스프라이트는 무영향, hit-flash/dissolve 는 재귀 ForEach 로 도달.
		BuildPlayerDirectionalGroups(*renderActor, *director);

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
			controller->SetFacingPivot(aimPivot); // facing 회전을 root 대신 aimPivot에 — 데칼 spin 분리(손은 aimPivot 궤도)
		}

		// VFX seam 주입 — 컴포넌트는 VFX 를 모르고, 빌더가 VFX::Spawn 람다를 주입 (director->Play 패턴).
		if (auto *life = spriteActor->GetComponent<Entity::Components::Life>())
			life->SetOnHitFx([](const vmath::vec3 &p) { VFX::Spawn("hit", p); })
				.SetOnDamageNumber([](int d, const vmath::vec3 &p) { WorldText::SpawnDamage(d, p); });
		if (auto *player = spriteActor->GetComponent<Entity::PlayerEntity>())
			player->SetOnMoveFx([](const vmath::vec3 &p) { VFX::Spawn("dust", p); });
		if (auto *weapon = spriteActor->GetComponent<Entity::Components::Weapon>())
			weapon->SetOnFireFx([](const vmath::vec3 &p, float yaw) { VFX::Spawn("gunshoot", p, yaw); });
		// ─────────────────────────────────────────────────────────────────────────

		// 발밑 그림자 + 피격범위 원 (groundActor 자식). std::move 전 = pre-entry.
		AttachGroundDecals(*spriteActor, PLAYER_DECAL_Y);

		result.SpriteActor = dir.Root().AddChild(std::move(spriteActor));

		// 손 — PlayerHands::OnEnter 가 자식 Hand actor 2개를 생성·부착 (player Y facing 상속 궤도).
		// SpriteActor 는 이미 entered -> 부착 즉시 OnEnter 실행. (손 스프라이트 비주얼은 사용자 WIP)
		result.SpriteActor->AddComponent<TopdownShooter::Entity::PlayerHands>(aimPivot); // 손 child는 aimPivot 아래 → 조준 궤도 유지

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
