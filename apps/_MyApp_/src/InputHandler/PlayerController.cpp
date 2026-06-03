
// PlayerController.h -> input/mouse_input.h 가 <GLFW/glfw3.h> 를 끌어오므로, GLFW 가 자체 GL 헤더를
// 포함해 엔진 gl3w 와 PFNGL* 가 충돌하지 않도록 *모든 include 이전* 에 NONE 을 선언한다.
#define GLFW_INCLUDE_NONE

#include "PlayerController.h"
#include "input/keyboard_input.h"
#include "object/transform.h"
#include "scene/actor.h"

// 마우스->Ground raycast + 발사/회전/디버그 마커에 필요한 의존 (Client 코드라 직접 사용 OK).
#include "Entity/BaseEntity.h"
#include "Entity/Components/WeaponComponents.h"
#include "Playable/Constants.h" // FacingThresholdConfig / PLAYER_FACING_THRESHOLD (헤더-only 데이터)
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "resource_registry/resource_registry.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <cmath>
#include <spdlog/spdlog.h>
#include <utility>
#include <vmath.h>

namespace
{
	// [start,end](degree)에 deg 포함? start>end 면 0° wrap.
	bool InRange(float deg, const vmath::vec2 &r)
	{
		return (r[0] <= r[1]) ? (deg >= r[0] && deg < r[1]) : (deg >= r[0] || deg < r[1]);
	}
	// dir=(x,z) -> θ=normalize360(deg(atan2(-z,x))) -> 4범위 중 포함 필드. no-match=fallback.
	TopdownShooter::Entity::EFacing QuantizeByThreshold(
	    vmath::vec2 dir, const TopdownShooter::Playable::FacingThresholdConfig &cfg,
	    TopdownShooter::Entity::EFacing fallback)
	{
		namespace E = TopdownShooter::Entity;
		float deg = std::atan2(-dir[1], dir[0]) * 57.29578f; // rad->deg, screen-up=-Z
		if (deg < 0.0f) deg += 360.0f;
		if (InRange(deg, cfg.Back)) return E::EFacing::Back;
		if (InRange(deg, cfg.Front)) return E::EFacing::Front;
		if (InRange(deg, cfg.Left)) return E::EFacing::Left;
		if (InRange(deg, cfg.Right)) return E::EFacing::Right;
		return fallback;
	}
}

namespace TopdownShooter::Controller
{
	void PlayerController::RegisterBindings()
	{
		// CameraController.cpp 의 BindKey + BindHeldHandler 2단 패턴을 그대로 모방.
		mKeyboardInput->BindKey(Action::MoveForward, GLFW_KEY_W);
		mKeyboardInput->BindKey(Action::MoveBack, GLFW_KEY_S);
		mKeyboardInput->BindKey(Action::MoveLeft, GLFW_KEY_A);
		mKeyboardInput->BindKey(Action::MoveRight, GLFW_KEY_D);

		// W = 앞 = -Z (OpenGL forward 컨벤션).
		// `+=` 누적 — 동시 키 (W+D 대각 등) 지원. Update 끝의 mInputValue=0 reset 이 매 프레임 보장.
		// 대각 √2 가속은 Movement::DoForward 의 normalize(dir) 가 자동 정규화.
		// (held 핸들러는 매 프레임 호출 -> 로그 스팸 방지 위해 discrete(G/클릭)만 로깅. spec §2.)
		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { mInputValue += vmath::vec3(0.0f, 0.0f, -1.0f); });
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { mInputValue += vmath::vec3(0.0f, 0.0f, 1.0f); });
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { mInputValue += vmath::vec3(-1.0f, 0.0f, 0.0f); });
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { mInputValue += vmath::vec3(1.0f, 0.0f, 0.0f); });

		// G키 (이산 press) — Damage Composite 트리거. 콜백은 호출 시점 null-check.
		mKeyboardInput->BindKey(Action::Damage, GLFW_KEY_G);
		mKeyboardInput->BindPressHandler(Action::Damage, [this] {
			spdlog::info("[input] G (Damage)");
			if (mDamageCallback)
				mDamageCallback();
		});

		// 좌클릭 (이산 press) — 발사. MouseInput 미주입이면 바인딩 생략.
		if (mMouseInput)
			mMouseInput->BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [this] { OnFirePressed(); });
	}

	void PlayerController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드) — 입력 의존은 non-null 보장.
		assert(this->mKeyboardInput != nullptr);

		mKeyboardInput->UnbindKey(GLFW_KEY_W);
		mKeyboardInput->UnbindKey(GLFW_KEY_S);
		mKeyboardInput->UnbindKey(GLFW_KEY_A);
		mKeyboardInput->UnbindKey(GLFW_KEY_D);
		mKeyboardInput->UnbindKey(GLFW_KEY_G);

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

	PlayerController &PlayerController::SetGroundClickCallback(std::function<void(const vmath::vec3 &)> cb)
	{
		mGroundClickCallback = std::move(cb);
		return *this;
	}

	PlayerController &PlayerController::SetFireCallback(std::function<void()> cb)
	{
		mFireCallback = std::move(cb);
		return *this;
	}

	PlayerController &PlayerController::SetDamageCallback(std::function<void()> cb)
	{
		mDamageCallback = std::move(cb);
		return *this;
	}

	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
	}

	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
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
			owner->GetTransform().EulerRot[1] = mAimAngleY; // 논리 facing — 자식 손이 WorldMatrix 로 상속

		// === RD5: facing/pose 단일 작성자 (controller 계산 -> sink 토글) ===
		if (mSink == nullptr && owner != nullptr)
			mSink = owner->GetComponent<TopdownShooter::Entity::IActorPresentation>(); // lazy(=director, 인터페이스 조회)
		if (mSink != nullptr)
		{
			namespace E = TopdownShooter::Entity;
			if (mAttackTimer > 0.0f) mAttackTimer -= dt;
			const bool        attacking = (mAttackTimer > 0.0f);
			const vmath::vec2 velXZ(mInputValue[0], mInputValue[2]); // ★ 리셋 전
			const bool        moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;
			const vmath::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);

			E::EFacing facing = attacking ? QuantizeByThreshold(aimXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                  : moving    ? QuantizeByThreshold(velXZ, TopdownShooter::Playable::PLAYER_FACING_THRESHOLD, mLastFacing)
			                              : mLastFacing;
			E::EPose pose = (attacking || moving) ? E::EPose::Move : E::EPose::Idle;
			mSink->SetFacing(facing);
			mSink->SetPose(pose);
			mLastFacing = facing;
		}

		// 누적값 리셋.
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

		mAttackTimer = mAttackWindowSec; // 발사 후 0.15s 동안 facing=조준 (하이브리드)

		spdlog::info("[fire] ground=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) angleY={:.1f}",
		             mAimPoint[0], mAimPoint[1], mAimPoint[2],
		             mAimDirection[0], mAimDirection[1], mAimDirection[2], mAimAngleY);

		// 발사 — owner 의 Weapon 경유 (spec D3). box2d forward = (aimDir.x, -aimDir.z).
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			auto *weapon = owner->GetComponent<Entity::Components::Weapon>();
			if (weapon != nullptr)
				weapon->UseWeapon(vmath::vec2(mAimDirection[0], -mAimDirection[2]));
		}

		// 오디오/VFX Composite (onFire) — 주입됐으면.
		if (mFireCallback)
			mFireCallback();

		// // 디버그 노란 마커 — 좌클릭에서만 스폰 (spec §2). 매 프레임 스폰 금지.
		// SpawnGroundMarker(mAimPoint);

		// Ground 좌표 소비자(VFX 소환 등) — 주입됐으면 클릭 위치 전달.
		if (mGroundClickCallback)
			mGroundClickCallback(mAimPoint);
	}

	// void PlayerController::SpawnGroundMarker(const vmath::vec3 &worldPos)
	// {
	// 	auto &reg = SJH::ResourceRegistry::Get();

	// 	// 박스 메시 (idempotent 캐시).
	// 	SJH::Mesh *mesh = reg.FindMesh("test_marker_box");
	// 	if (mesh == nullptr)
	// 		mesh = reg.RegisterMesh("test_marker_box", SJH::Mesh::CreateBox());

	// 	// 노란 단색 머티리얼 — simple.vs/fs(baseColor) + Opaque.
	// 	SJH::Material *mat = reg.FindSharedMaterial("test_marker_yellow");
	// 	if (mat == nullptr)
	// 	{
	// 		SJH::Program *prog = reg.FindProgram("test_solid");
	// 		if (prog == nullptr)
	// 			prog = reg.CreateProgram("test_solid", "resources/shaders/simple.vs", "resources/shaders/simple.fs");
	// 		mat = reg.CreateSharedMaterial("test_marker_yellow");
	// 		mat->SetProgram(prog);
	// 		mat->SetPass(SJH::Pass::Kind::Opaque);
	// 		SJH::Uniforms::SetVec4(*mat, "baseColor", vmath::vec4(1.0f, 0.95f, 0.1f, 1.0f));
	// 	}

	// 	auto marker = std::make_unique<SJH::Scene::Actor>("GroundMarker");
	// 	marker->GetTransform().Translate = worldPos;
	// 	marker->GetTransform().Scale = vmath::vec3(0.4f, 0.4f, 0.4f);
	// 	marker->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
	// 	SJH::Scene::Director::Get().Root().AddChild(std::move(marker));
	// }
} // namespace TopdownShooter::Controller
