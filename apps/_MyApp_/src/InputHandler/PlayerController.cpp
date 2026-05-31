
// PlayerController.h → input/mouse_input.h 가 <GLFW/glfw3.h> 를 끌어오므로, GLFW 가 자체 GL 헤더를
// 포함해 엔진 gl3w 와 PFNGL* 가 충돌하지 않도록 *모든 include 이전* 에 NONE 을 선언한다.
#define GLFW_INCLUDE_NONE

#include "PlayerController.h"
#include "input/keyboard_input.h"
#include "object/transform.h"
#include "scene/actor.h"

// [TEST] 마우스→Ground raycast + 노란 마커 스폰에 필요한 의존 (Client 코드라 직접 사용 OK).
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
		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { mInputValue += vmath::vec3(0.0f, 0.0f, -1.0f); spdlog::info("[input] W (MoveForward)"); });
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { mInputValue += vmath::vec3(0.0f, 0.0f, 1.0f); spdlog::info("[input] S (MoveBack)"); });
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { mInputValue += vmath::vec3(-1.0f, 0.0f, 0.0f); spdlog::info("[input] A (MoveLeft)"); });
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { mInputValue += vmath::vec3(1.0f, 0.0f, 0.0f); spdlog::info("[input] D (MoveRight)"); });

		// G키 (이산 press) — Damage Composite 트리거. 콜백은 호출 시점 null-check.
		mKeyboardInput->BindKey(Action::Damage, GLFW_KEY_G);
		mKeyboardInput->BindPressHandler(Action::Damage, [this] {
			spdlog::info("[input] G (Damage)");
			if (mDamageCallback)
				mDamageCallback();
		});

		// 좌클릭 (이산 press) — Shot Composite 트리거. MouseInput 미주입이면 바인딩 생략.
		if (mMouseInput)
			mMouseInput->BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [this] {
				spdlog::info("[input] LMB (Fire)");
				// [TEST] 마우스→Ground raycast → 좌표 로그 + 노란 마커 스폰.
				TestPickGroundAndSpawnMarker();
				if (mFireCallback)
					mFireCallback();
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
		// dt 는 Movement::DoForward 가 units/sec  프레임 변위로 변환 (fps-independent).
		mMovementPtr->DoForward({mInputValue[0], mInputValue[2]}, dt);
		// 누적값 리셋.
		mInputValue = vmath::vec3(0.0f);
	}

	void PlayerController::TestPickGroundAndSpawnMarker()
	{
		if (mCamera == nullptr || mCamera->GetOwner() == nullptr)
		{
			spdlog::warn("[pick] World 카메라 미주입 — raycast 생략");
			return;
		}
		GLFWwindow *win = glfwGetCurrentContext();
		if (win == nullptr)
			return;

		double mx = 0.0, my = 0.0;
		glfwGetCursorPos(win, &mx, &my);
		int ww = 0, wh = 0;
		glfwGetWindowSize(win, &ww, &wh);
		if (ww <= 0 || wh <= 0)
			return;

		// 화면(픽셀) → NDC. y 는 위가 +1 이 되도록 뒤집는다.
		const float ndcX = 2.0f * static_cast<float>(mx) / static_cast<float>(ww) - 1.0f;
		const float ndcY = 1.0f - 2.0f * static_cast<float>(my) / static_cast<float>(wh);

		// 카메라 world basis — owner WorldMatrix 의 컬럼. (vmath 는 일반 inverse 미제공 →
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

		// y=0 평면과 교차. dir.y ≈ 0 이면 평행, t<0 이면 카메라 뒤 → 무시.
		if (std::fabs(dir[1]) < 1e-5f)
		{
			spdlog::warn("[pick] ray 가 y=0 평면과 평행 — 교차 없음");
			return;
		}
		const float t = -camPos[1] / dir[1];
		if (t < 0.0f)
		{
			spdlog::warn("[pick] y=0 교차가 카메라 뒤 — 무시");
			return;
		}
		const vmath::vec3 hit = camPos + dir * t;

		spdlog::info("[pick] screen=({:.0f},{:.0f}) ndc=({:.2f},{:.2f}) -> ground=({:.2f},{:.2f},{:.2f})",
		             mx, my, ndcX, ndcY, hit[0], hit[1], hit[2]);

		SpawnGroundMarker(hit);
	}

	void PlayerController::SpawnGroundMarker(const vmath::vec3 &worldPos)
	{
		auto &reg = SJH::ResourceRegistry::Get();

		// 박스 메시 (idempotent 캐시).
		SJH::Mesh *mesh = reg.FindMesh("test_marker_box");
		if (mesh == nullptr)
			mesh = reg.RegisterMesh("test_marker_box", SJH::Mesh::CreateBox());

		// 노란 단색 머티리얼 — simple.vs/fs(baseColor) + Opaque.
		SJH::Material *mat = reg.FindSharedMaterial("test_marker_yellow");
		if (mat == nullptr)
		{
			SJH::Program *prog = reg.FindProgram("test_solid");
			if (prog == nullptr)
				prog = reg.CreateProgram("test_solid", "resources/shaders/simple.vs", "resources/shaders/simple.fs");
			mat = reg.CreateSharedMaterial("test_marker_yellow");
			mat->SetProgram(prog);
			mat->SetPass(SJH::Pass::Kind::Opaque);
			SJH::Uniforms::SetVec4(*mat, "baseColor", vmath::vec4(1.0f, 0.95f, 0.1f, 1.0f));
		}

		auto marker = std::make_unique<SJH::Scene::Actor>("GroundMarker");
		marker->GetTransform().Translate = worldPos;
		marker->GetTransform().Scale = vmath::vec3(0.4f, 0.4f, 0.4f);
		marker->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
		SJH::Scene::Director::Get().Root().AddChild(std::move(marker));
	}
} // namespace TopdownShooter::Controller
