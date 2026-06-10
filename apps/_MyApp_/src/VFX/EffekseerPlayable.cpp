/**
 * @file EffekseerPlayable.cpp
 * @brief EffekseerPlayable 구현 - Effekseer Effect 1개의 재생 인스턴스 lifecycle 관리 leaf Playable.
 *
 * @details
 *  ### 구현 흐름
 *  1. ctor - VFXSystem 이 생성한 @c ManagerRef 를 *빌림* (소유 안 함). @c SJH::Effect 도 비소유.
 *     manager 또는 effect 가 nullptr 이면 경고 로그 후 기능 off.
 *  2. dtor - Handle 이 유효(>= 0)하면 @c StopEffect 로 인스턴스 정지 후 -1 마킹.
 *     * Handle 은 정수 ID 라 RAII 가 없다 - dtor 에서 반드시 수동 정리해야 파티클 leak 방지.
 *  3. @c OnPlay - @c StartHandle 호출 -> @c EffekseerDiagnostics 로 Play 실패/즉시종료 진단.
 *     * .efkefc export 버전이 런타임(1.7.3.0) 지원 버전보다 높으면 Play 가 -1 을 반환한다.
 *  4. @c OnStop - Handle 유효 시 @c StopEffect + 무효화 (-1).
 *  5. @c OnUpdate - 3단계 갱신:
 *     a. SpinRadPerSec != 0 이면 @c SetRotation 으로 yaw 갱신 (궁극기 회전 레이저).
 *     b. @c TrackPolicy::FollowOwner 이면 @c SetLocation 으로 Actor Transform 추적.
 *     c. MaxDurationSec 경과 시 강제 종료 (루프보다 우선). 아니면 @c Exists 로 자연 종료 감지.
 *
 *  ### 비-책임
 *  - [X] Manager/Renderer 소유 - @c VFXSystem 담당.
 *  - [X] .efkefc 로드/캐시 - @c SJH::ResourceRegistry / @c SJH::Effect 담당.
 *  - [X] 시뮬레이션 Update/렌더 Draw - @c VFXSystem / @c ParticleStage 담당.
 */
#include "EffekseerPlayable.h"

#include "diagnostics/effekseer_diagnostics.h"   // Effekseer 핸들 lifecycle 진단
#include "scene/actor.h"        // GetOwner() (FollowOwner 정책에서 Transform 조회)
#include <spdlog/spdlog.h>

namespace TopdownShooter::VFX
{
	/// @brief 생성자.
	/// @details
	///  @p manager 는 @c VFXSystem::GetManager() 에서 빌림 (소유 안 함, Ref 타입 참조 카운트만 증가).
	///  @p effect 는 @c SJH::ResourceRegistry 가 소유한 캐시 entry 포인터 - 비소유.
	///  manager 또는 effect nullptr 시 이후 OnPlay 에서 early return 으로 무시된다.
	/// @param manager  VFXSystem 이 소유한 Effekseer::ManagerRef (빌림).
	/// @param effect   SJH::Effect 포인터 - EffectRef 를 Play 에 전달하기 위한 캐시 entry (비소유).
	/// @param spawnPos 이펙트 생성 위치 (월드 좌표).
	/// @param track    위치 추적 정책 (@c TrackPolicy::Static or @c FollowOwner).
	/// @param yawRad   Y축 초기 회전각(라디안). 0 이면 회전 없음.
	EffekseerPlayable::EffekseerPlayable(::Effekseer::ManagerRef manager,
	                                     SJH::Effect            *effect,
	                                     const vmath::vec3      &spawnPos,
	                                     TrackPolicy             track,
	                                     float                   yawRad)
	    : mManager(manager), mEffect(effect), mSpawnPos(spawnPos), mTrack(track), mYaw(yawRad)
	{
		if (mManager.Get() == nullptr || mEffect == nullptr)
			spdlog::warn("[EffekseerPlayable] ctor: manager 또는 effect nullptr");
	}

	/// @brief 소멸자.
	/// @details
	///  * Handle 은 정수 ID 라 RAII 가 없다. dtor 에서 직접 @c StopEffect 를 호출하지 않으면
	///  파티클이 계속 재생되거나 dangling 참조가 된다.
	///  OnStop 을 먼저 호출한 경우 mHandle 이 -1 이므로 중복 StopEffect 는 발생하지 않는다.
	EffekseerPlayable::~EffekseerPlayable()
	{
		if (mManager.Get() != nullptr && mHandle >= 0)
		{
			mManager->StopEffect(mHandle);
			mHandle = -1;
		}
	}

	/// @brief 이펙트를 @c mSpawnPos 에 Play 하고 @c mHandle 에 인스턴스 ID 를 보관.
	/// @details
	///  @c OnPlay 와 루프 재생(@c OnUpdate 자연종료 분기) 에서 공유하는 내부 헬퍼.
	///  @n mYaw != 0 이면 @c SetRotation(handle, rx=0, ry=mYaw, rz=0) 으로 Y축 회전 주입.
	///  @n * Play 가 -1 을 반환하는 주요 원인 - .efkefc export 버전이
	///  런타임 지원 버전(1710)보다 높을 때. 에러/로그 없이 조용히 실패.
	void EffekseerPlayable::StartHandle()
	{
		mHandle = mManager->Play(mEffect->Ref(),
		                         ::Effekseer::Vector3D(mSpawnPos[0], mSpawnPos[1], mSpawnPos[2]));
		// Y축 회전 주입(라디안 오일러) - 발사 방향 등. 0 이면 생략.
		if (mYaw != 0.0f && mHandle >= 0)
			mManager->SetRotation(mHandle, 0.0f, mYaw, 0.0f);
	}

	/// @brief PlayableBase::Play() 훅 - 이펙트 재생 시작.
	/// @details
	///  @c StartHandle 로 Play + Y축 회전 주입.
	///  @n 진단 - @c EffekseerDiagnostics::CheckPlayHandle (Play 실패 = -1 감지) +
	///  @c CheckHandleAlive (Play 직후 즉시 종료 = 빈 이펙트/텍스처 전무 감지).
	void EffekseerPlayable::OnPlay()
	{
		if (mManager.Get() == nullptr || mEffect == nullptr) return;
		StartHandle();
		// 진단 - Play 실패(-1) / Play 직후 즉시 종료(빈 이펙트/텍스처 전무) 감지.
		SJH::Diagnostics::EffekseerDiagnostics::CheckPlayHandle(mHandle, "effekseer");
		SJH::Diagnostics::EffekseerDiagnostics::CheckHandleAlive(
		    mHandle, mManager->Exists(mHandle), "effekseer");
	}

	/// @brief PlayableBase::Stop() 훅 - 이펙트 즉시 정지.
	/// @details
	///  * @c StopEffect 후 @p mHandle 을 -1 로 무효화. dtor 중복 호출 방지.
	void EffekseerPlayable::OnStop()
	{
		if (mManager.Get() != nullptr && mHandle >= 0)
		{
			mManager->StopEffect(mHandle);
			mHandle = -1;
		}
	}

	/// @brief PlayableBase::Update() 훅 - 매 프레임 위치 추적 + 종료 감지.
	/// @details
	///  갱신 우선순위:
	///  1. SpinRadPerSec != 0 -> @c SetRotation (yaw = mYaw + mSpinRadPerSec * mElapsed).
	///  2. TrackPolicy::FollowOwner -> @c SetLocation (Actor::GetTransform().Translate 절대 위치).
	///  3. MaxDurationSec > 0 이고 @p mElapsed >= @p mMaxDurationSec -> 강제 종료 (루프보다 우선).
	///  4. @c Exists(mHandle) 이 false -> 자연 종료. 루프면 @c StartHandle 재발동, 아니면 finished.
	///  @n @c dt 파라미터는 사용하지 않음 (시간 누적은 PlayableBase::mElapsed 가 책임).
	/// @param dt 직전 프레임 경과 시간(초) - 미사용(PlayableBase 가 mElapsed 갱신).
	void EffekseerPlayable::OnUpdate(float /*dt*/)
	{
		if (mManager.Get() == nullptr) return;

		// 회전 - 매 프레임 yaw 갱신 (시작각 mYaw + 회전속도x경과). 궁극기 회전 레이저 등.
		if (mSpinRadPerSec != 0.0f && mHandle >= 0)
			mManager->SetRotation(mHandle, 0.0f, mYaw + mSpinRadPerSec * mElapsed, 0.0f);

		// FollowOwner - Actor 의 Transform.Translate 를 매 frame 추적
		if (mTrack == TrackPolicy::FollowOwner && mHandle >= 0)
		{
			if (auto *owner = GetOwner())
			{
				const auto &p = owner->GetTransform().Translate;
				mManager->SetLocation(mHandle, ::Effekseer::Vector3D(p[0], p[1], p[2]));
			}
		}

		// 최대 지속 경과 - 강제 종료 + finished (AutoDespawnOnFinish 가 sweep). 루프 재생보다 우선.
		if (mMaxDurationSec > 0.0f && mElapsed >= mMaxDurationSec)
		{
			if (mHandle >= 0)
			{
				mManager->StopEffect(mHandle);
				mHandle = -1;
			}
			mIsFinished = true;
			return;
		}

		// 자연 종료 처리 - Effect 가 더 이상 존재 안 함.
		if (mHandle >= 0 && !mManager->Exists(mHandle))
		{
			if (mIsLoop)
				StartHandle();          // 루프 - 재생 반복 (orbital 상시 회전 / 3초 미만 laser 반복 등)
			else
			{
				mHandle     = -1;
				mIsFinished = true;
			}
		}
	}
}
