
// PlayerController.h -> input/mouse_input.h 가 <GLFW/glfw3.h> 를 끌어오므로, GLFW 가 자체 GL 헤더를
// 포함해 엔진 gl3w 와 PFNGL* 가 충돌하지 않도록 *모든 include 이전* 에 NONE 을 선언한다.
#include "Entity/Player/PlayerEntity.h"
#define GLFW_INCLUDE_NONE

#include "PlayerController.h"
#include "input/keyboard_input.h"
#include "object/transform.h"
#include "scene/actor.h"

// 마우스->Ground raycast + 발사/회전/디버그 마커에 필요한 의존 (Client 코드라 직접 사용 OK).
#include "Entity/BaseEntity.h"
#include "Entity/Components/WeaponComponents.h"
#include "Entity/Player/PlayerHand.h" // 발사 핀치 — 좌클릭 바인딩에서 PlayerHands::TriggerFire 통지
#include "Playable/Constants.h"       // FacingThresholdConfig / PLAYER_FACING_THRESHOLD (헤더-only 데이터)
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "Spawns/VfxInstance.h" // VFX::Spawn (R키 laser VFX 테스트)
#include "resource_registry/resource_registry.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <cmath>
#include <spdlog/spdlog.h>
#include <utility>
#include <vmath.h>

namespace TopdownShooter::Controller
{
	// !  이 부분은 PlayerSprite Playable로 리팩토링 해야함.
	bool InRange(float deg, const vmath::vec2 &r)
	{
		return (r[0] <= r[1]) ? (deg >= r[0] && deg < r[1]) : (deg >= r[0] || deg < r[1]);
	}
	// !  이 부분은 PlayerSprite Playable로 리팩토링 해야함.
	// !dir=(x,z) -> θ=normalize360(deg(atan2(-z,x))) -> 4범위 중 포함 필드. no-match=fallback.
	TopdownShooter::Entity::EFacing QuantizeByThreshold(
	    vmath::vec2 dir, const TopdownShooter::Playable::FacingThresholdConfig &cfg,
	    TopdownShooter::Entity::EFacing fallback)
	{
		namespace E = TopdownShooter::Entity;
		float deg = std::atan2(-dir[1], dir[0]) * 57.29578f; // rad->deg, screen-up=-Z
		if (deg < 0.0f)
			deg += 360.0f;
		if (InRange(deg, cfg.Back))
			return E::EFacing::Back;
		if (InRange(deg, cfg.Front))
			return E::EFacing::Front;
		if (InRange(deg, cfg.Left))
			return E::EFacing::Left;
		if (InRange(deg, cfg.Right))
			return E::EFacing::Right;
		return fallback;
	}

	void PlayerController::RegisterBindings()
	{
		// CameraController.cpp 의 BindKey + BindHeldHandler 2단 패턴을 그대로 모방.
		mKeyboardInput->BindKey(Action::MoveForward, GLFW_KEY_W);
		mKeyboardInput->BindKey(Action::MoveBack, GLFW_KEY_S);
		mKeyboardInput->BindKey(Action::MoveLeft, GLFW_KEY_A);
		mKeyboardInput->BindKey(Action::MoveRight, GLFW_KEY_D);
		mKeyboardInput->BindKey(Action::DashImpulse, GLFW_KEY_LEFT_SHIFT);
		mKeyboardInput->BindKey(Action::Ultimate, GLFW_KEY_R);

		// W = 앞 = -Z (OpenGL forward 컨벤션).
		// `+=` 누적 — 동시 키 (W+D 대각 등) 지원. Update 끝의 mInputValue=0 reset 이 매 프레임 보장.
		// 대각 √2 가속은 Movement::DoForward 의 normalize(dir) 가 자동 정규화.
		// (held 핸들러는 매 프레임 호출 -> 로그 스팸 방지 위해 discrete(G/클릭)만 로깅. spec §2.)
		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { 
			mInputValue += vmath::vec3(0.0f, 0.0f, -1.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { 
			mInputValue += vmath::vec3(0.0f, 0.0f, 1.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { 
			mInputValue += vmath::vec3(-1.0f, 0.0f, 0.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { 
			mInputValue += vmath::vec3(1.0f, 0.0f, 0.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::DashImpulse, [this] {
			if (auto *owner = GetOwner())
			{
				const vmath::vec2 aimXZ(mPrevInputValue[0], mPrevInputValue[2]);
				if(auto* pe = owner->GetComponent<Entity::PlayerEntity>()) {
					pe->Dash(aimXZ); 
					return;
				}
			}
			spdlog::error("NO FIND IMPULSE");
		});
		// R(Ultimate) — 단순 VFX 테스트: 플레이어 위치에 blue_laser(Constants.h "laser" 키) 스폰.
		// held 가 아니라 press(이산 1회) — 단발 이펙트라 매 프레임 폭주 방지 (좌클릭 발사와 동일 결).
		mKeyboardInput->BindPressHandler(Action::Ultimate, [this] {
			if (auto *owner = GetOwner())
			{
				spdlog::info("[input] R (Ultimate) — laser VFX 테스트");
				VFX::Spawn("laser", owner->GetTransform().Translate);
			}
		});

		// 좌클릭 (이산 press) — 발사. MouseInput 미주입이면 바인딩 생략.
		if (mMouseInput)
			mMouseInput->BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [this] {
				OnFirePressed();
				// 발사 핀치 통지 — 좁힘/복귀 로직은 PlayerHands 가 소유. 입력 바인딩은 호출만(멤버 캐시 없음).
				if (auto *owner = GetOwner())
					if (auto *hands = owner->GetComponent<Entity::PlayerHands>())
						hands->TriggerFire();
			});
	}

	void PlayerController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드) — 입력 의존은 non-null 보장.
		assert(this->mKeyboardInput != nullptr);

		mKeyboardInput->UnbindKey(GLFW_KEY_W);
		mKeyboardInput->UnbindKey(GLFW_KEY_S);
		mKeyboardInput->UnbindKey(GLFW_KEY_A);
		mKeyboardInput->UnbindKey(GLFW_KEY_D);

		if (mMouseInput)
			mMouseInput->UnbindButtonPress(GLFW_MOUSE_BUTTON_LEFT);
	}

	bool PlayerController::SetUp()
	{
		if (!mKeyboardInput)
		{
			spdlog::error("PlayerController::SetUp — KeyboardInput 미주입");
			return false;
		}
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	PlayerController &PlayerController::SetKeyboardInput(SJH::KeyboardInput<Action> *k)
	{
		// 멱등 — 두 번째 호출은 무시. nullptr 검증은 SetUp() 한 곳에서.
		if (mKeyboardInput == nullptr)
			mKeyboardInput = k;
		return *this;
	}

	PlayerController &PlayerController::SetMovableTarget(Entity::IMovable *target)
	{
		// 멱등 — 첫 비-null 주입 후 무시.
		if (mMovementPtr == nullptr)
			mMovementPtr = target;
		return *this;
	}

	PlayerController &PlayerController::SetMouseInput(SJH::MouseInput *m)
	{
		// 멱등 — 첫 비-null 주입 후 무시. RegisterBindings 가 좌클릭 바인딩 시점에 참조.
		if (mMouseInput == nullptr)
			mMouseInput = m;
		return *this;
	}

	PlayerController &PlayerController::SetWorldCamera(SJH::Scene::Camera *cam)
	{
		// 멱등 — 첫 비-null 주입 후 무시. 미주입이면 좌클릭 raycast 생략.
		if (mCamera == nullptr)
			mCamera = cam;
		return *this;
	}

	PlayerController &PlayerController::SetFacingPivot(SJH::Scene::Actor *pivot)
	{
		// 멱등 — 첫 비-null 주입 후 무시.
		if (mFacingPivot == nullptr)
			mFacingPivot = pivot;
		return *this;
	}

	PlayerController &PlayerController::SetFireCallback(std::function<void()> cb)
	{
		mFireCallback = std::move(cb);
		return *this;
	}

	PlayerController &PlayerController::SetDamageCallback(std::function<void()> cb)
	{
		return *this;
	}

	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
		// attack 윈도 timer 를 BaseEntity 중앙 컨테이너에 등록 + arm-inactive (발사 시 Reset 으로 발동).
		if (auto *owner = GetOwner())
		{
			mEntity = owner->GetComponent<TopdownShooter::Entity::BaseEntity>();
			if (mEntity != nullptr)
			{
				mAttackTimer = mEntity->Timers().Register("player.attack", mAttackWindowSec);
				mAttackTimer->Tick(mAttackTimer->GetBaseTime());
			}
		}
	}

	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
		if (mEntity != nullptr)
			mEntity->Timers().Unregister("player.attack");
		mAttackTimer = nullptr;
		UnregisterBindings();
		mKeyboardInput = nullptr;
		mIsInitialized = false;
	}

	void PlayerController::Update(float dt)
	{
		if (!mIsInitialized)
			return;

		// facade lazy 캐시 (controller 가 facade 보다 먼저 OnEnter 될 수 있어 첫 Update 에서 조회).
		if (mEntity == nullptr && GetOwner() != nullptr)
			mEntity = GetOwner()->GetComponent<TopdownShooter::Entity::BaseEntity>();

		// 대시(Impulse) 중에는 입력 자유이동을 Block — DoForward 호출 자체를 skip.
		// (입력 0 이어도 DoForward(0) 이 속도를 0 으로 만들어 버스트를 죽이므로 호출 자체를 막아야 함.)
		// 현재 dash 입력 미배선이라 IsImpulseActive()=false -> 게이트 dormant(행동 변화 0).
		const bool impulseActive = (mEntity != nullptr && mEntity->IsImpulseActive());
		if (!impulseActive)
			mMovementPtr->DoForward({mInputValue[0], mInputValue[2]}, dt);

		// === 연속 조준 (spec D1) — 매 프레임 마우스->Ground raycast 로 조준 멤버 갱신. ===
		UpdateAim();

		// === 플레이어 회전 (spec D2/§3) — 논리 facing(EulerRot.Y). ===
		// mAimAngleY/mAimDirection 은 직전 유효값을 유지하므로 mAimValid 와 무관하게 매 프레임 반영.
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			// facing 회전은 pivot(주입 시)에만 적용 — root는 비회전(데칼 spin 분리).
			// 미주입이면 owner(하위호환). aimPivot 하위 손이 WorldMatrix 로 회전 상속.
			SJH::Scene::Actor *pivot = (mFacingPivot != nullptr) ? mFacingPivot : owner;
			pivot->GetTransform().EulerRot[1] = mAimAngleY;
		}

		// === RD5: facing/pose 단일 작성자 (controller 계산 -> sink 토글) ===
		if (mSink == nullptr && owner != nullptr)
			mSink = owner->GetComponent<TopdownShooter::Entity::IActorPresentation>(); // lazy(=director, 인터페이스 조회)
		if (mSink != nullptr)
		{
			namespace E = TopdownShooter::Entity;
			const bool attacking = (mAttackTimer != nullptr && !mAttackTimer->IsTimesUp()); // tick은 BaseEntity가
			const vmath::vec2 velXZ(mInputValue[0], mInputValue[2]);                        // ★ 리셋 전
			const bool moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;
			const vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);

			E::EFacing facing = attacking ? QuantizeByThreshold(aimXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                    : moving  ? QuantizeByThreshold(velXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                              : mLastFacing;
			E::EPose pose = (attacking || moving) ? E::EPose::Move : E::EPose::Idle;
			mSink->SetFacing(facing);
			mSink->SetPose(pose);
			mLastFacing = facing;
		}

		// 누적값 리셋.
		mPrevInputValue = mInputValue;
		mInputValue = vmath::vec3(0.0f);
	}

	bool PlayerController::UpdateAim()
	{
		// 카메라 미주입 / 윈도우 없음 — 직전 조준 유지 (silent).
		if (mCamera == nullptr || mCamera->GetOwner() == nullptr)
		{
			mAimValid = false;
			return false;
		}
		GLFWwindow *win = glfwGetCurrentContext();
		if (win == nullptr)
		{
			mAimValid = false;
			return false;
		}

		double mx = 0.0, my = 0.0;
		glfwGetCursorPos(win, &mx, &my);
		int ww = 0, wh = 0;
		glfwGetWindowSize(win, &ww, &wh);
		if (ww <= 0 || wh <= 0)
		{
			mAimValid = false;
			return false;
		}

		// 화면(픽셀) -> NDC. y 는 위가 +1 이 되도록 뒤집는다.
		const float ndcX = 2.0f * static_cast<float>(mx) / static_cast<float>(ww) - 1.0f;
		const float ndcY = 1.0f - 2.0f * static_cast<float>(my) / static_cast<float>(wh);

		// 카메라 world basis — owner WorldMatrix 의 컬럼. (vmath 는 일반 inverse 미제공 ->
		// proj·view 역행렬 대신 fov/aspect 로 view-space ray 를 직접 구성해 world 로 회전.)
		const vmath::mat4 camW = mCamera->GetOwner()->GetWorldMatrix();
		const vmath::vec3 right(camW[0][0], camW[0][1], camW[0][2]);
		const vmath::vec3 up(camW[1][0], camW[1][1], camW[1][2]);
		const vmath::vec3 forward(-camW[2][0], -camW[2][1], -camW[2][2]); // -Z 컬럼 = forward
		const vmath::vec3 camPos(camW[3][0], camW[3][1], camW[3][2]);

		const float tanHalf = std::tan(vmath::radians(mCamera->FovYDeg * 0.5f));
		const float aspect = mCamera->Aspect;
		const vmath::vec3 dir =
		    normalize(right * (ndcX * aspect * tanHalf) + up * (ndcY * tanHalf) + forward);

		// === 화면(NDC) 정규화 조준 강도 mAimScreenT — 플레이어를 NDC 에 투영해 커서 NDC 와의 거리. ===
		// 손 spread 보간용. 화면 가장자리(NDC 1.0)에서 포화(1). ground 교차 성공 여부와 무관(여기서 미리 산출).
		// mat*vec 미지원(vmath) → 커서 ray 와 동일 basis/규약으로 직접 투영:
		//   depth = dot(rel, forward), ndc = dot(rel, right|up) / (depth * (aspect)tanHalf).
		if (SJH::Scene::Actor *pl = GetOwner())
		{
			const vmath::vec3 rel = pl->GetTransform().Translate - camPos;
			const float depth = vmath::dot(rel, forward); // view forward 깊이 (>0 = 카메라 앞)
			if (depth > 1e-4f)
			{
				const float pNdcX = vmath::dot(rel, right) / (depth * aspect * tanHalf);
				const float pNdcY = vmath::dot(rel, up) / (depth * tanHalf);
				const float sdx = ndcX - pNdcX;
				const float sdy = ndcY - pNdcY;
				const float st = std::sqrt(sdx * sdx + sdy * sdy);
				mAimScreenT = (st > 1.0f) ? 1.0f : st; // 화면 가장자리에서 포화
			}
		}

		// y=0 평면과 교차. dir.y ≈ 0 이면 평행, t<0 이면 카메라 뒤 -> 직전값 유지.
		if (std::fabs(dir[1]) < 1e-5f)
		{
			mAimValid = false;
			return false;
		}
		const float t = -camPos[1] / dir[1];
		if (t < 0.0f)
		{
			mAimValid = false;
			return false;
		}
		const vmath::vec3 hit = camPos + dir * t;

		// === PlayerActor(owner) -> 커서 Ground 좌표 = 조준 Vector 추출 (XZ 평면) ===
		SJH::Scene::Actor *player = GetOwner();
		const vmath::vec3 playerPos = (player != nullptr) ? player->GetTransform().Translate : vmath::vec3(0.0f);

		vmath::vec3 aim = hit - playerPos;
		aim[1] = 0.0f; // 탑다운 조준 — 높이 성분 제거 (XZ 평면)
		const float dist = vmath::length(aim);

		mAimPoint = hit;
		mAimDistance = dist; // 손 spread 보간 등 거리 소비자용 (방향이 무효여도 거리는 유효)
		mAimValid = true;
		if (dist > 1e-4f)
		{
			mAimDirection = aim * (1.0f / dist);
			// facing Y각 (degree) — spec §1: θ = degrees(atan2(-dir.x, -dir.z)). forward(-Z)=0, +X=-90.
			mAimAngleY = vmath::degrees(std::atan2(-mAimDirection[0], -mAimDirection[2]));
		}
		// dist≈0 (커서가 player 위) — 방향/각도는 직전값 유지 (snap 방지). mAimPoint 만 갱신.
		return true;
	}

	void PlayerController::OnFirePressed()
	{
		// 클릭 직전 조준 갱신 — 입력 디스패치가 Update 보다 앞설 수 있어 커서 최신값으로 재산출.
		UpdateAim();

		if (mAttackTimer)
			mAttackTimer->Reset(); // 발사 후 0.15s 동안 facing=조준 (하이브리드)

		spdlog::info("[fire] ground=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) angleY={:.1f}",
		             mAimPoint[0], mAimPoint[1], mAimPoint[2],
		             mAimDirection[0], mAimDirection[1], mAimDirection[2], mAimAngleY);

		// 발사 — owner 의 Weapon 경유 (spec D3). box2d forward = (aimDir.x, -aimDir.z).
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			auto *pe = owner->GetComponent<Entity::PlayerEntity>();
			if (pe != nullptr)
				pe->UseWeapon(vmath::vec2(mAimDirection[0], -mAimDirection[2]));
		}

		// 오디오/VFX Composite (onFire) — 주입됐으면.
		if (mFireCallback)
			mFireCallback();
	}

} // namespace TopdownShooter::Controller
