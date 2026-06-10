/**
 * @file SpriteFxPlayable.h
 * @brief 스프라이트 셰이더 FX 를 구동하는 leaf Playable 2종 - hit-flash / dissolve.
 *
 * @details
 *  ### 책임
 *  - @c SpriteHitFlashPlayable : 대상 액터(+서브트리) 의 모든 @c SpriteRenderer.enableHit 를
 *    지정 시간 동안 true 로 유지 -> 셰이더(billboard_atlas) 가 uTime 클럭으로 플래시 애니.
 *  - @c SpriteDissolvePlayable : dissolveThreshold 를 0->1 로 선형 구동 -> 사라짐.
 *    노이즈 텍스처(dissolve.png)를 ResourceRegistry 에 캐시해 공유 사용.
 *
 *  ### 비-책임
 *  - [X] 셰이더 구현 (uEnableHit / uDissolveThreshold / uDissolveTex) - billboard_atlas.frag 담당.
 *  - [X] Playable 등록/재생 제어 - @c PlayableDirector 담당.
 *  - [X] 사망 처리(body 비활성/despawn) - Life 컴포넌트 담당.
 *
 *  ### 정통 매핑
 *  - Unity @c Material.SetFloat("_DissolveAmount", t) 를 Playable 로 래핑한 패턴.
 *
 * @note PropertyBlockSetter 가 GL_BOOL uniform 디스패치를 지원해야 enableHit/enableDissolve 가
 *       실제 업로드된다. 미지원 시 셰이더 값이 영영 갱신되지 않아 FX 가 무반응한다.
 * @note dissolveTex 미주입 시 unit0(=atlas)를 fallback 사용 - 스프라이트 내용 의존 crude erode.
 *       반드시 @c SpriteDissolvePlayable 이 노이즈 텍스처를 주입해야 균일한 디졸브 보장.
 */
#ifndef __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__
#define __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__

#include "playable/playable_base.h"
#include "Playable/Constants.h"

// fwd - 대상 액터는 포인터만 보유 (자식 SpriteRenderer 를 .cpp 에서 수집).
namespace SJH::Scene
{
	class Actor;
}

namespace TopdownShooter::Playable
{
	/**
	 * @brief [C] 피격 hit-flash leaf Playable.
	 * @details 대상 액터(+서브트리) 의 모든 @c SpriteRenderer.enableHit 를 @p durationSec 동안 true 로 유지.
	 *  셰이더(billboard_atlas) 가 자체 uTime 클럭으로 플래시 애니를 구동한다. one-shot.
	 *  @c PlayableDirector "hit" 컴포지트에 합성. player/enemy 공용 (target 만 바꿔 재사용 - P4).
	 */
	class SpriteHitFlashPlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief 생성자.
		/// @param target      FX 를 적용할 루트 액터 (서브트리 전체 대상).
		/// @param durationSec 플래시 지속 시간 (초). 기본값 @c SPRITE_HIT_FLASH_DURATION.
		explicit SpriteHitFlashPlayable(SJH::Scene::Actor *target, float durationSec = SPRITE_HIT_FLASH_DURATION);
		~SpriteHitFlashPlayable() override;

	  protected:
		/// @brief 재생 시작 - 서브트리 전체 @c enableHit = true.
		void OnPlay() override;

		/// @brief 정지/리셋 - 서브트리 전체 @c enableHit = false.
		void OnStop() override;

		/// @brief 매 프레임 - @c durationSec 경과 시 @c enableHit = false + @c mIsFinished = true.
		/// @param dt 프레임 경과 시간 (초, 미사용 - @c mElapsed 는 PlayableBase 가 누적).
		void OnUpdate(float dt) override;

	  private:
		SJH::Scene::Actor *mTarget;    ///< FX 루트 액터 (non-owning).
		float              mDuration;  ///< 플래시 지속 시간 (초).
	};

	/**
	 * @brief [C] 사망 dissolve leaf Playable.
	 * @details 대상 액터(+서브트리) @c SpriteRenderer 의 @c enableDissolve = true + @c dissolveThreshold 를
	 *  0 -> 1 로 @p durationSec 에 걸쳐 선형 구동(사라짐). one-shot - 완료 후 dissolved 상태 유지.
	 *  @c PlayableDirector "death" 컴포지트에 합성. player/enemy 공용 (P4 재사용).
	 *
	 *  노이즈 텍스처(dissolve.png) 를 @c ResourceRegistry 에 캐시해 인스턴스 간 공유 사용.
	 *  미주입 시 unit0(=atlas) fallback -> 스프라이트 내용 의존 crude erode (적 즉시 사라짐 증상).
	 */
	class SpriteDissolvePlayable : public SJH::Playable::PlayableBase
	{
	  public:
		/// @brief 생성자.
		/// @param target      dissolve 를 적용할 루트 액터 (서브트리 전체 대상).
		/// @param durationSec 디졸브 지속 시간 (초). 기본값 @c SPRITE_DISSOLVE_DURATION.
		explicit SpriteDissolvePlayable(SJH::Scene::Actor *target, float durationSec = SPRITE_DISSOLVE_DURATION);
		~SpriteDissolvePlayable() override;

	  protected:
		/// @brief 재생 시작 - @c enableDissolve = true, @c dissolveThreshold = 0, 노이즈 텍스처 주입.
		void OnPlay() override;

		/// @brief 정지/리셋 - @c enableDissolve = false, @c dissolveThreshold = 0.
		void OnStop() override;

		/// @brief 매 프레임 - @c dissolveThreshold = clamp(elapsed/duration, 0, 1).
		///        threshold 가 1.0 에 도달하면 @c mIsFinished = true (dissolved 상태 유지, despawn 은 Life 책임).
		/// @param dt 프레임 경과 시간 (초, 미사용 - @c mElapsed 는 PlayableBase 가 누적).
		void OnUpdate(float dt) override;

	  private:
		SJH::Scene::Actor *mTarget;    ///< dissolve 루트 액터 (non-owning).
		float              mDuration;  ///< 디졸브 지속 시간 (초).
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_SPRITE_FX_PLAYABLE_H__
