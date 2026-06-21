/**
 * @file PlayerController.cpp
 * @brief Top-down 플레이어 입력 컨트롤러 구현 - WASD dt-독립 이동 + 마우스 조준/발사 + 대시/궁극기.
 *
 * @details
 *  ### 구현 책임
 *  - @c RegisterBindings / @c UnregisterBindings -- @c KeyboardInput<Action> + @c MouseInput 에
 *    액션별 키/버튼을 바인딩/해제.
 *  - @c Update -- 누적된 @c mInputValue 로 @c IMovable::DoForward 호출(dt 독립 이동) +
 *    @c UpdateAim 으로 조준 정보 갱신 + facing/pose sink 토글.
 *  - @c UpdateAim -- 마우스 커서 픽셀 좌표를 NDC 로 변환 -> 카메라 basis 기반 ray 구성 ->
 *    y=0 Ground 평면 교차 -> @c mAimPoint / @c mAimDirection / @c mAimAngleY 갱신.
 *  - @c OnFirePressed -- 좌클릭 시 조준 최신화 + Weapon 발사 + @c mFireCallback 실행.
 *
 *  ### 이동 흐름
 *  BindHeldHandler(W/A/S/D) 가 매 프레임 @c mInputValue 에 방향 벡터를 누적. @c Update 말미에
 *  @c IMovable::DoForward({x, z}, dt) 호출 후 @c mInputValue = 0 리셋. 대각(W+D) 누적은
 *  DoForward 내부 @c normalize 가 자동 정규화 -> sqrt(2) 가속 없음.
 *
 *  ### 조준(raycast) 흐름
 *  카메라 FOV + aspect 로 view-space ray 직접 구성(proj*view 역행렬 생략) -> y=0 평면과 교차점 t 산출 ->
 *  player-owner 위치 기준 방향/각도 추출. dist ~= 0 이면 snap 방지를 위해 방향/각도는 직전값 유지.
 *
 *  ### 비-책임
 *  - [X] 발사체/연출 생성 - Weapon Component / Spawns 자유 함수 / 콜백 위임.
 *  - [X] 타이머 tick - @c BaseEntity 중앙 컨테이너(@c Timers()) 가 유일 tick.
 *  - [X] facing 스프라이트 교체 - @c IActorPresentation sink(@c PlayableDirector) 위임.
 *
 * @note GLFW GL 헤더 충돌 방지를 위해 @c GLFW_INCLUDE_NONE 을 *모든 include 이전* 에 정의 (아래 include 순서 보존 필수).
 */

// PlayerController.h -> input/mouse_input.h 가 <GLFW/glfw3.h> 를 끌어오므로, GLFW 가 자체 GL 헤더를
// 포함해 엔진 gl3w 와 PFNGL* 가 충돌하지 않도록 *모든 include 이전* 에 NONE 을 선언한다.
#define GLFW_INCLUDE_NONE

#include "PlayerController.h"
#include "input/keyboard_input.h"
#include "object/transform.h"
#include "scene/actor.h"

// 마우스->Ground raycast + 발사/회전/디버그 마커에 필요한 의존 (Client 코드라 직접 사용 OK).
#include "Contracts/EntityContracts.h" // IPlayerCommand/IFireTrigger/ITimerOwner/IImpulseState (C2 DIP - 구체 Entity 미참조)
#include "timer/timer.h"               // SJH::Timer::Timer (mAttackTimer->Tick/GetBaseTime 완전형)
#include "timer/multiple_timer.h"      // SJH::Timer::MultipleTimer (ITimerOwner::Timers() Register/Unregister 완전형)
#include "Playable/Constants.h"       // FacingThresholdConfig / PLAYER_FACING_THRESHOLD (헤더-only 데이터)
#include "material/material.h"
#include "material/material_uniforms.h"
#include "object/mesh.h"
#include "program/program.h"
#include "render/mesh_renderer.h"
#include "Spawns/UltimateLaser.h" // Spawns::SpawnUltimateLaser (R키 궁극기 회전 히트스캔 레이저)
#include "resource_registry/resource_registry.h"
#include "scene/camera.h"
#include "scene/scene.h"

#include <GLFW/glfw3.h>
#include <cassert>
#include <cmath>
#include <spdlog/spdlog.h>
#include <utility>
#include <glm/glm.hpp>

namespace TopdownShooter::Controller
{
	/// @brief 각도 @p deg 가 범위 @p r 안에 포함되는지 판정 (360도 wrap 고려).
	/// @details r[0] <= r[1] 이면 일반 구간. r[0] > r[1] 이면 0도를 넘는 wrap-around 구간
	///          (예: [315, 45) -- 후방 범위).
	/// @param deg 판정할 각도 (degree, 0~360).
	/// @param r   [min, max) 구간 (r[0]=min, r[1]=max).
	/// @return @p deg 가 구간 안이면 true.
	// !  이 부분은 PlayerSprite Playable로 리팩토링 해야함.
	bool InRange(float deg, const glm::vec2 &r)
	{
		return (r[0] <= r[1]) ? (deg >= r[0] && deg < r[1]) : (deg >= r[0] || deg < r[1]);
	}
	/// @brief XZ 방향 벡터를 4방향 @c EFacing 으로 양자화.
	/// @details
	///   방향 벡터 @p dir (x, z) 를 각도로 변환 후 @c FacingThresholdConfig 의 4개 구간과 대조.
	///   dir=(x,z) -> theta = normalize360(degrees(atan2(-z, x))) -> Back/Front/Left/Right 중 포함 반환.
	///   no-match 이면 @p fallback 반환. wrap-around 구간(후방)은 @c InRange 내부에서 처리.
	/// @param dir      XZ 방향 벡터 (정규화 여부 무관).
	/// @param cfg      EFacing 별 [min, max) 각도 구간 설정.
	/// @param fallback 어떤 구간에도 해당하지 않을 때 반환할 기본 방향.
	/// @return 판정된 @c EFacing 열거값.
	// !  이 부분은 PlayerSprite Playable로 리팩토링 해야함.
	// !dir=(x,z) -> theta=normalize360(deg(atan2(-z,x))) -> 4범위 중 포함 필드. no-match=fallback.
	TopdownShooter::Entity::EFacing QuantizeByThreshold(
	    glm::vec2 dir, const TopdownShooter::Playable::FacingThresholdConfig &cfg,
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

	/// @brief 키보드/마우스 바인딩 등록 내부 구현.
	/// @details
	///   @c mKeyboardInput->BindKey + @c BindHeldHandler 2단 패턴 (CameraController 정통):
	///   (1) @c BindKey -- GLFW 키 코드를 @c Action 에 매핑.
	///   (2) @c BindHeldHandler -- 누름 유지 중 매 프레임 호출되는 람다 등록.
	///   WASD 는 held 방향 누적(mInputValue +=), Shift 는 held 대시, R/클릭은 press(이산) 바인딩.
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
		// `+=` 누적 - 동시 키 (W+D 대각 등) 지원. Update 끝의 mInputValue=0 reset 이 매 프레임 보장.
		// 대각 sqrt2 가속은 Movement::DoForward 의 normalize(dir) 가 자동 정규화.
		// (held 핸들러는 매 프레임 호출 -> 로그 스팸 방지 위해 discrete(G/클릭)만 로깅. spec sec.2.)
		mKeyboardInput->BindHeldHandler(Action::MoveForward, [this] { 
			mInputValue += glm::vec3(0.0f, 0.0f, -1.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveBack, [this] { 
			mInputValue += glm::vec3(0.0f, 0.0f, 1.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveLeft, [this] { 
			mInputValue += glm::vec3(-1.0f, 0.0f, 0.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::MoveRight, [this] { 
			mInputValue += glm::vec3(1.0f, 0.0f, 0.0f); 
		});
		mKeyboardInput->BindHeldHandler(Action::DashImpulse, [this] {
			if (auto *owner = GetOwner())
			{
				const glm::vec2 aimXZ(mPrevInputValue[0], mPrevInputValue[2]);
				if(auto* pe = owner->GetComponent<Entity::IPlayerCommand>()) {
					pe->Dash(aimXZ);
					return;
				}
			}
			spdlog::error("NO FIND IMPULSE");
		});
		// R(Ultimate) - 플레이어 중심 회전 히트스캔 레이저 궁극기 (3초 유지 + 1초당 1회전 + 자동 파괴).
		// 시작각 = 마우스 조준. 매 프레임 RaycastAll 로 경로상 적에 적별 0.2s 틱 데미지. press(이산 1회).
		mKeyboardInput->BindPressHandler(Action::Ultimate, [this] {
			UpdateAim(); // 최신 조준 (입력 디스패치가 Update 보다 앞설 수 있어 커서 최신값 재산출)
			if (auto *owner = GetOwner())
			{
				// box2d forward = (aimDir.x, -aimDir.z) -> 시작 회전각 = atan2(fwd.y, fwd.x).
				const float startAngle = std::atan2(-mAimDirection[2], mAimDirection[0]);
				spdlog::info("[input] R (Ultimate) - 회전 레이저 발동 angle={:.1f}deg", startAngle * 57.29578f);
				Spawns::SpawnUltimateLaser(mWorld, owner->GetTransform().Translate, startAngle, owner);
			}
		});

		// 좌클릭 (이산 press) - 발사. MouseInput 미주입이면 바인딩 생략.
		if (mMouseInput)
			mMouseInput->BindButtonPressHandler(GLFW_MOUSE_BUTTON_LEFT, [this] {
				OnFirePressed();
				// 발사 핀치 통지 - 좁힘/복귀 로직은 PlayerHands 가 소유. 입력 바인딩은 호출만(멤버 캐시 없음).
				if (auto *owner = GetOwner())
					if (auto *hands = owner->GetComponent<Entity::IFireTrigger>())
						hands->TriggerFire();
			});
	}

	/// @brief 키보드/마우스 바인딩 해제 내부 구현.
	/// @details @c OnExit 의 @c mIsInitialized 가드를 통과한 경우에만 호출됨 -- 입력 의존(non-null) 보장.
	///   WASD 키 + 좌클릭 버튼의 바인딩을 @c KeyboardInput / @c MouseInput 에서 제거.
	void PlayerController::UnregisterBindings()
	{
		// SetUp 이 성공한 경우만 호출됨 (OnExit 의 mIsInitialized 가드) - 입력 의존은 non-null 보장.
		assert(this->mKeyboardInput != nullptr);

		mKeyboardInput->UnbindKey(GLFW_KEY_W);
		mKeyboardInput->UnbindKey(GLFW_KEY_S);
		mKeyboardInput->UnbindKey(GLFW_KEY_A);
		mKeyboardInput->UnbindKey(GLFW_KEY_D);

		if (mMouseInput)
			mMouseInput->UnbindButtonPress(GLFW_MOUSE_BUTTON_LEFT);
	}

	/// @brief 키보드 입력 의존 검증 후 바인딩 등록 (Component lifecycle -- 첫 활성화 전 1회).
	/// @details @c mKeyboardInput 이 null 이면 error 로그 후 false 반환.
	///   성공 시 @c RegisterBindings 를 호출하고 @c mIsInitialized = true 로 설정.
	///   이후 @c Update / @c OnEnter / @c OnExit 는 @c mIsInitialized 가드로 보호됨.
	/// @return true -- 셋업 성공(이동+입력 정상), false -- KeyboardInput 미주입.
	bool PlayerController::SetUp()
	{
		if (!mKeyboardInput)
		{
			spdlog::error("PlayerController::SetUp - KeyboardInput 미주입");
			return false;
		}
		RegisterBindings();
		mIsInitialized = true;
		return true;
	}

	/// @brief @c KeyboardInput<Action> 의존 주입 (멱등 -- 첫 null 상태일 때만 저장).
	/// @param k 주입할 KeyboardInput 포인터. null 허용, 검증은 @c SetUp() 에서 수행.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetKeyboardInput(SJH::KeyboardInput<Action> *k)
	{
		// 멱등 - 두 번째 호출은 무시. nullptr 검증은 SetUp() 한 곳에서.
		if (mKeyboardInput == nullptr)
			mKeyboardInput = k;
		return *this;
	}

	/// @brief @c IMovable 구현체 의존 주입 (멱등 -- 첫 null 상태일 때만 저장).
	/// @param target Movement Component 또는 Entity 자체의 @c IMovable 구현 포인터. null 비허용.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetMovableTarget(Entity::IMovable *target)
	{
		// 멱등 - 첫 비-null 주입 후 무시.
		if (mMovementPtr == nullptr)
			mMovementPtr = target;
		return *this;
	}

	/// @brief 마우스 입력 의존 주입 (멱등 -- 선택적). 좌클릭 Fire 바인딩에 사용.
	/// @param m 주입할 MouseInput 포인터. null 이면 좌클릭 바인딩 전체 생략.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetMouseInput(SJH::MouseInput *m)
	{
		// 멱등 - 첫 비-null 주입 후 무시. RegisterBindings 가 좌클릭 바인딩 시점에 참조.
		if (mMouseInput == nullptr)
			mMouseInput = m;
		return *this;
	}

	/// @brief World 카메라 주입 (멱등 -- 선택적). 마우스->Ground raycast 에 사용.
	/// @param cam 씬 World 카메라 포인터. null 이면 @c UpdateAim 에서 raycast 생략.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetWorldCamera(SJH::Scene::Camera *cam)
	{
		// 멱등 - 첫 비-null 주입 후 무시. 미주입이면 좌클릭 raycast 생략.
		if (mCamera == nullptr)
			mCamera = cam;
		return *this;
	}

	/// @brief facing 회전 pivot Actor 주입 (멱등 -- 선택적).
	/// @details 미주입이면 owner(root Actor) 에 직접 EulerRot[1] 적용 (하위호환).
	///   주입 시 root 는 비회전 유지, @p pivot 만 회전 -> 데칼(그림자) spin 분리 가능.
	/// @param pivot 회전 대상 Actor 포인터.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetFacingPivot(SJH::Scene::Actor *pivot)
	{
		// 멱등 - 첫 비-null 주입 후 무시.
		if (mFacingPivot == nullptr)
			mFacingPivot = pivot;
		return *this;
	}

	/// @brief Box2D 물리 월드 주입 (멱등 -- 선택적). 궁극기(R) 회전 히트스캔 레이저용.
	/// @param world b2World 포인터. null 이면 R 궁극기 @c SpawnUltimateLaser 내부 guard 로 no-op.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetWorld(b2World *world)
	{
		// 멱등 - 첫 비-null 주입 후 무시. 미주입이면 R 궁극기 no-op (SpawnUltimateLaser 내부 guard).
		if (mWorld == nullptr)
			mWorld = world;
		return *this;
	}

	/// @brief 좌클릭(발사) 직후 실행할 콜백 주입. 오디오/VFX Composite 등 연결용.
	/// @param cb 발사 성공 시 호출될 콜백. null 이면 콜백 무시.
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetFireCallback(std::function<void()> cb)
	{
		mFireCallback = std::move(cb);
		return *this;
	}

	/// @brief G키(피격 테스트) 콜백 주입 -- 현재 미사용(stub).
	/// @param cb 피격 시 호출될 콜백 (현재 미배선 -- 바디는 no-op).
	/// @return @c *this (fluent 체이닝).
	PlayerController &PlayerController::SetDamageCallback(std::function<void()> cb)
	{
		return *this;
	}

	/// @brief Component 활성화 훅 -- attack 윈도 타이머를 @c BaseEntity 중앙 컨테이너에 등록.
	/// @details @c mIsInitialized 가 false 이면 즉시 반환 (SetUp 미호출 상태 방어).
	///   owner 에서 @c BaseEntity 를 조회 후 "player.attack" 키로 @c mAttackWindowSec 기반 타이머를
	///   @c Timers().Register 에 등록. 등록 직후 @c Tick(BaseTime) 으로 arm-inactive 상태로 초기화 --
	///   발사 시 @c Reset 으로 카운트다운 발동.
	void PlayerController::OnEnter()
	{
		if (!mIsInitialized)
			return;
		// attack 윈도 timer 를 ITimerOwner 중앙 컨테이너에 등록 + arm-inactive (발사 시 Reset 으로 발동).
		if (auto *owner = GetOwner())
		{
			mTimerOwner = owner->GetComponent<TopdownShooter::Entity::ITimerOwner>();
			if (mTimerOwner != nullptr)
			{
				mAttackTimer = mTimerOwner->Timers().Register("player.attack", mAttackWindowSec);
				mAttackTimer->Tick(mAttackTimer->GetBaseTime());
			}
		}
	}

	/// @brief Component 비활성화 훅 -- 타이머 해제 + 바인딩 해제 + 상태 초기화.
	/// @details @c mIsInitialized 가 false 이면 즉시 반환.
	///   "player.attack" 타이머를 @c Timers().Unregister 로 해제한 뒤 핸들 null 초기화.
	///   @c UnregisterBindings 로 WASD/좌클릭 바인딩 해제 후 @c mKeyboardInput / @c mIsInitialized 리셋.
	void PlayerController::OnExit()
	{
		if (!mIsInitialized)
			return;
		if (mTimerOwner != nullptr)
			mTimerOwner->Timers().Unregister("player.attack");
		mAttackTimer = nullptr;
		UnregisterBindings();
		mKeyboardInput = nullptr;
		mIsInitialized = false;
	}

	/// @brief 매 프레임 입력 처리 -- 이동/조준/facing 갱신.
	/// @details 처리 순서:
	///   (1) @c mImpulseState lazy 캐시 -- controller 가 facade 보다 먼저 @c OnEnter 될 수 있어 첫 Update 에서 조회.
	///   (2) dash(Impulse) 활성 중이면 @c IMovable::DoForward 호출 skip (대시 버스트 보존).
	///   (3) @c UpdateAim 으로 마우스->Ground raycast 조준 정보 갱신.
	///   (4) facing pivot 의 EulerRot[1] 에 @c mAimAngleY 적용 (논리 회전).
	///   (5) attack/move 상태로 @c EFacing/@c EPose 를 산출 -> @c IActorPresentation sink 에 설정.
	///   (6) @c mPrevInputValue 저장 후 @c mInputValue = 0 리셋.
	/// @param dt 프레임 델타 타임 (초 단위, dt-독립 이동용).
	void PlayerController::Update(float dt)
	{
		if (!mIsInitialized)
			return;

		// 임펄스 게이트 lazy 캐시 (controller 가 facade 보다 먼저 OnEnter 될 수 있어 첫 Update 에서 조회).
		if (mImpulseState == nullptr && GetOwner() != nullptr)
			mImpulseState = GetOwner()->GetComponent<TopdownShooter::Entity::IImpulseState>();

		// 대시(Impulse) 중에는 입력 자유이동을 Block - DoForward 호출 자체를 skip.
		// (입력 0 이어도 DoForward(0) 이 속도를 0 으로 만들어 버스트를 죽이므로 호출 자체를 막아야 함.)
		// 현재 dash 입력 미배선이라 IsImpulseActive()=false -> 게이트 dormant(행동 변화 0).
		const bool impulseActive = (mImpulseState != nullptr && mImpulseState->IsImpulseActive());
		if (!impulseActive)
			mMovementPtr->DoForward({mInputValue[0], mInputValue[2]}, dt);

		// === 연속 조준 (spec D1) - 매 프레임 마우스->Ground raycast 로 조준 멤버 갱신. ===
		UpdateAim();

		// === 플레이어 회전 (spec D2/sec.3) - 논리 facing(EulerRot.Y). ===
		// mAimAngleY/mAimDirection 은 직전 유효값을 유지하므로 mAimValid 와 무관하게 매 프레임 반영.
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			// facing 회전은 pivot(주입 시)에만 적용 - root는 비회전(데칼 spin 분리).
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
			const glm::vec2 velXZ(mInputValue[0], mInputValue[2]);                        // * 리셋 전
			const bool moving = (velXZ[0] * velXZ[0] + velXZ[1] * velXZ[1]) > 0.001f;
			const glm::vec2 aimXZ(mAimDirection[0], mAimDirection[2]);

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
		mInputValue = glm::vec3(0.0f);
	}

	/// @brief 마우스 커서를 Ground(y=0) 평면에 raycast 해 조준 멤버를 갱신.
	/// @details
	///   흐름:
	///   (1) 사전 조건 확인 -- mCamera null / GLFW window null / window 크기 0 이면 mAimValid=false 로 조기 반환.
	///   (2) 픽셀 좌표 -> NDC 변환 (ndcX, ndcY). y 는 위=+1 로 뒤집음.
	///   (3) 카메라 WorldMatrix 컬럼에서 right/up/forward/camPos 추출.
	///   (4) FOV + aspect 로 view-space ray 를 직접 구성 (proj*view 역행렬 없이도 동일 결과):
	///       dir = normalize(right*(ndcX*aspect*tanHalf) + up*(ndcY*tanHalf) + forward).
	///   (5) mAimScreenT 산출 -- player NDC 위치 <-> 커서 NDC 거리 (0=중심, 1=화면 가장자리 포화).
	///   (6) y=0 평면 교차 -- t = -camPos.y / dir.y. dir.y ~= 0 또는 t < 0 이면 조기 반환.
	///   (7) hit = camPos + dir*t -- mAimPoint, mAimDistance 갱신.
	///   (8) dist > 1e-4 이면 mAimDirection / mAimAngleY 갱신. dist ~= 0 이면 직전값 유지 (snap 방지).
	///
	/// @note @c Update 및 @c OnFirePressed 에서 각각 1회씩 호출됨 -- 입력 디스패치가 Update 보다
	///       앞설 수 있어 클릭 직전 최신화 필요.
	/// @return true -- 유효 교차 성공 / false -- 카메라 미주입 / 윈도우 없음 / ray 평행 / t<0.
	bool PlayerController::UpdateAim()
	{
		// 카메라 미주입 / 윈도우 없음 - 직전 조준 유지 (silent).
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

		// 카메라 world basis - owner WorldMatrix 의 컬럼. (proj/view 역행렬을 쓰지 않고
		// fov/aspect 로 view-space ray 를 직접 구성해 world 로 회전 - 역행렬 회피.)
		const glm::mat4 camW = mCamera->GetOwner()->GetWorldMatrix();
		const glm::vec3 right(camW[0][0], camW[0][1], camW[0][2]);
		const glm::vec3 up(camW[1][0], camW[1][1], camW[1][2]);
		const glm::vec3 forward(-camW[2][0], -camW[2][1], -camW[2][2]); // -Z 컬럼 = forward
		const glm::vec3 camPos(camW[3][0], camW[3][1], camW[3][2]);

		const float tanHalf = std::tan(glm::radians(mCamera->FovYDeg * 0.5f));
		const float aspect = mCamera->Aspect;
		const glm::vec3 dir =
		    glm::normalize(right * (ndcX * aspect * tanHalf) + up * (ndcY * tanHalf) + forward);

		// === 화면(NDC) 정규화 조준 강도 mAimScreenT - 플레이어를 NDC 에 투영해 커서 NDC 와의 거리. ===
		// 손 spread 보간용. 화면 가장자리(NDC 1.0)에서 포화(1). ground 교차 성공 여부와 무관(여기서 미리 산출).
		// 커서 ray 와 동일 basis/규약으로 직접 투영 (mat*vec 대신 dot 분해):
		//   depth = dot(rel, forward), ndc = dot(rel, right|up) / (depth * (aspect)tanHalf).
		if (SJH::Scene::Actor *pl = GetOwner())
		{
			const glm::vec3 rel = pl->GetTransform().Translate - camPos;
			const float depth = glm::dot(rel, forward); // view forward 깊이 (>0 = 카메라 앞)
			if (depth > 1e-4f)
			{
				const float pNdcX = glm::dot(rel, right) / (depth * aspect * tanHalf);
				const float pNdcY = glm::dot(rel, up) / (depth * tanHalf);
				const float sdx = ndcX - pNdcX;
				const float sdy = ndcY - pNdcY;
				const float st = std::sqrt(sdx * sdx + sdy * sdy);
				mAimScreenT = (st > 1.0f) ? 1.0f : st; // 화면 가장자리에서 포화
			}
		}

		// y=0 평면과 교차. dir.y ~= 0 이면 평행, t<0 이면 카메라 뒤 -> 직전값 유지.
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
		const glm::vec3 hit = camPos + dir * t;

		// === PlayerActor(owner) -> 커서 Ground 좌표 = 조준 Vector 추출 (XZ 평면) ===
		SJH::Scene::Actor *player = GetOwner();
		const glm::vec3 playerPos = (player != nullptr) ? player->GetTransform().Translate : glm::vec3(0.0f);

		glm::vec3 aim = hit - playerPos;
		aim[1] = 0.0f; // 탑다운 조준 - 높이 성분 제거 (XZ 평면)
		const float dist = glm::length(aim);

		mAimPoint = hit;
		mAimDistance = dist; // 손 spread 보간 등 거리 소비자용 (방향이 무효여도 거리는 유효)
		mAimValid = true;
		if (dist > 1e-4f)
		{
			mAimDirection = aim * (1.0f / dist);
			// facing Y각 (degree) - spec sec.1: theta = degrees(atan2(-dir.x, -dir.z)). forward(-Z)=0, +X=-90.
			mAimAngleY = glm::degrees(std::atan2(-mAimDirection[0], -mAimDirection[2]));
		}
		// dist~=0 (커서가 player 위) - 방향/각도는 직전값 유지 (snap 방지). mAimPoint 만 갱신.
		return true;
	}

	/// @brief 좌클릭 발사 액션 처리 -- 조준 최신화 + Weapon 발사 + 콜백 실행.
	/// @details
	///   (1) @c UpdateAim 으로 클릭 직전 조준 정보를 최신화 (입력 디스패치가 Update 앞설 수 있음).
	///   (2) @c mAttackTimer->Reset 으로 "조준 응시 윈도 (0.15s)" 카운트다운 발동.
	///   (3) spdlog::info 로 ground 좌표/방향/각도 로깅 (디버그).
	///   (4) owner 의 @c PlayerEntity 를 통해 @c UseWeapon(box2d forward) 호출 --
	///       box2d forward = (aimDir.x, -aimDir.z) (spec D3, 좌표계 변환).
	///   (5) @c mFireCallback 실행 (오디오/VFX Composite 등).
	void PlayerController::OnFirePressed()
	{
		// 클릭 직전 조준 갱신 - 입력 디스패치가 Update 보다 앞설 수 있어 커서 최신값으로 재산출.
		UpdateAim();

		if (mAttackTimer)
			mAttackTimer->Reset(); // 발사 후 0.15s 동안 facing=조준 (하이브리드)

		spdlog::info("[fire] ground=({:.2f},{:.2f},{:.2f}) dir=({:.2f},{:.2f},{:.2f}) angleY={:.1f}",
		             mAimPoint[0], mAimPoint[1], mAimPoint[2],
		             mAimDirection[0], mAimDirection[1], mAimDirection[2], mAimAngleY);

		// 발사 - owner 의 Weapon 경유 (spec D3). box2d forward = (aimDir.x, -aimDir.z).
		SJH::Scene::Actor *owner = GetOwner();
		if (owner != nullptr)
		{
			auto *pe = owner->GetComponent<Entity::IPlayerCommand>();
			if (pe != nullptr)
				pe->UseWeapon(glm::vec2(mAimDirection[0], -mAimDirection[2]));
		}

		// 오디오/VFX Composite (onFire) - 주입됐으면.
		if (mFireCallback)
			mFireCallback();
	}

} // namespace TopdownShooter::Controller
