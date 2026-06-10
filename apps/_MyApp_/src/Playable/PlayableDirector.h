/**
 * @file PlayableDirector.h
 * @brief named Playable 을 보유/tick 하는 연출 디렉터 컴포넌트.
 *
 * @details
 *  ### 책임
 *  - verb -> @c Play(key) 패턴으로 named Playable 을 중앙 등록/재생/정지.
 *  - @c Component::Update(dt) 로 활성(Play 된) 슬롯만 직접 tick (MultipleTimer 식 중앙 디스패치).
 *  - @c IActorPresentation 구현: Life 의 sink(ReactDamaged/ReactDied/ReactAttack) 를 받아
 *    대응 Playable key 를 재생.
 *  - 4방향 x 2포즈 = 8그룹 @c SpriteRenderer 레이어의 가시성 토글 (SetFacing/SetPose).
 *
 *  ### 비-책임
 *  - [X] 게임 로직(HP/damage/physics/AI/입력판정) - Entity/State 계층 담당.
 *  - [X] leaf Playable 생성/배선 - 빌더(PlayerBuilder 등) 가 Register 로 주입.
 *  - [X] IPlayable Composite 구조(Sequence/Parallel) - src/playable 담당.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c ActionManager : named action 보유 + per-node tick.
 *  - Unity @c Animator : trigger(verb) -> clip 재생.
 *
 * @note Sequence(EffekseerPlayable -> 사운드) 구성 시 efk 핸들 무효이면 @c IsFinished 가
 *       영영 false 가 되어 hang 발생. 동시 연출은 반드시 @c ParallelPlayable 사용.
 */
#ifndef __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__
#define __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__

#include "Entity/Components/Components.Interfaces.h" // IActorPresentation / EFacing / EPose (sink)
#include "playable/playable_base.h"                  // SJH::Playable::PlayableBase (map 보유 + Component)
#include "scene/actor.h"                             // SJH::Scene::Component

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vmath.h>

namespace SJH::Sprite { class SpriteRenderer; } // DirGroup 핸들 (실타입 .cpp)

namespace TopdownShooter::Playable
{
	/**
	 * @brief 연출 디렉터 - named Playable 보유 + 중앙 tick.
	 * @details
	 *  철학: 게임 로직(HP/damage/physics/AI/입력판정)은 별개. 모든 연출/비주얼/사운드는
	 *  이 director 의 named Playable 로만 (verb->Play(key)).
	 *
	 *  다중 상속 구조:
	 *  - @c SJH::Scene::Component : 액터에 부착되어 scene tick 을 받아 보유 Playable 을 직접 Update.
	 *  - @c IActorPresentation    : Life 의 sink (Life::DoDamaged->ReactDamaged / DoDie->ReactDied).
	 *
	 *  보유 Playable 은 AddComponent 하지 않고 director 가 직접 @c Update(dt) 한다
	 *  (@c MultipleTimer 가 @c Timer 를 Tick 하듯). @c Play() 로 활성화된 슬롯만 tick -
	 *  등록 직후 미재생 슬롯은 tick 제외(자동재생 방지).
	 *  leaf/Composite Playable 자체는 @c src/playable + Client 거주.
	 *
	 *  정통 매핑: Cocos2D @c ActionManager (named action + per-node tick).
	 */
	class PlayableDirector : public SJH::Scene::Component, public Entity::IActorPresentation
	{
	  public:
		/// @brief @p key 로 Playable 등록 (소유 이관). 같은 키 재등록은 덮어쓰기. fluent.
		/// @param key   named Playable 식별자 ("hit" / "death" / "attack" 등).
		/// @param p     등록할 Playable (unique_ptr, 소유권 이관). nullptr 이면 등록되지만 Play 시 no-op.
		/// @return *this (fluent 체인).
		PlayableDirector &Register(const std::string &key, std::unique_ptr<SJH::Playable::PlayableBase> p);

		/// @brief @p key 슬롯을 Stop()+Play() - 첫 프레임부터 재생 + 슬롯 활성화.
		/// @details 미등록 키는 silent no-op.
		/// @param key 재생할 named Playable 식별자.
		void Play(const std::string &key);

		/// @brief @p key 슬롯을 Stop() 후 슬롯 비활성(다음 Play 까지 tick 제외).
		/// @details 미등록 키는 no-op.
		/// @param key 정지할 named Playable 식별자.
		void Stop(const std::string &key);

		/// @brief @p key 가 등록되어 있으면 true.
		/// @param key 확인할 named Playable 식별자.
		/// @return 등록 여부.
		bool Has(const std::string &key) const;

		/**
		 * @brief (facing, pose) 한 그룹 - 해당 방향/포즈의 4-레이어 @c SpriteRenderer 핸들.
		 * @details 4 레이어 = body/shadow/arm/muzzle 등 조합 (구성은 빌더 책임).
		 *          레이어 중 사용하지 않는 슬롯은 nullptr 로 남긴다.
		 */
		struct DirGroup { std::array<SJH::Sprite::SpriteRenderer *, 4> layers{}; };

		/// @brief (facing, pose) 조합의 레이어 그룹 등록 (빌드 시 1회).
		/// @param f 방향 열거.
		/// @param p 포즈 열거.
		/// @param g 해당 조합에 대한 @c DirGroup.
		void RegisterGroup(Entity::EFacing f, Entity::EPose p, const DirGroup &g) { mGroups[idx(f)][idx(p)] = g; }

		/// @brief AddChild(OnEnter) 후 1회 호출 - 초기 (Front, Idle) 그룹만 활성화.
		void RefreshDirectional() { Apply(); }

		// === Component hook ===
		void OnEnter() override {}  ///< 현재 추가 초기화 없음.
		void OnExit() override {}   ///< 현재 추가 해제 없음.

		/// @brief 중앙 tick - @c Play() 로 활성화되고 미완료인 슬롯만 Update(dt).
		/// @details 완료된 슬롯은 자동으로 비활성 처리 (다음 @c Play() 까지 휴면).
		/// @param dt 프레임 경과 시간 (초).
		void Update(float dt) override;

		// === IActorPresentation - verb->Play(key) (미등록 키는 silent no-op) ===

		/// @brief 피격 반응 - "hit" Playable 재생.
		/// @param dmg 피해량 (현재 미사용 - FX 트리거 목적).
		void ReactDamaged(int /*dmg*/) override { Play("hit"); }

		/// @brief 사망 반응 - "death" Playable 재생.
		/// @details 역할별 분리(P2): director 는 [C] 화면 연출 담당. 월드점 death FX[B](폭발 등)는
		///          도메인 seam(Life::mOnDeathFx)이 @c Spawns/ 로 직접 트리거.
		/// @param pos 월드 사망 위치 (현재 director 는 미사용 - 도메인 seam 이 처리).
		void ReactDied(vmath::vec3 pos) override;

		/// @brief 공격 반응 - "attack" Playable 재생.
		/// @param aimDir 조준 방향 (현재 미사용 - FX 트리거 목적).
		void ReactAttack(vmath::vec2 /*aimDir*/) override { Play("attack"); }

		/// @brief controller(RD5) 가 계산한 facing 으로 8그룹 가시성 토글.
		/// @param f 새 facing. 변경 없으면 early return.
		void SetFacing(Entity::EFacing f) override;

		/// @brief controller(RD5) 가 계산한 pose 로 8그룹 가시성 토글.
		/// @param p 새 pose. 변경 없으면 early return.
		void SetPose(Entity::EPose p) override;

	  private:
		/**
		 * @brief 한 named Playable 슬롯.
		 * @details @c playing == true 일 때만 매 프레임 tick. @c Play() 로 활성화, @c Stop() 또는
		 *          @c IsFinished() 시 비활성. 등록 직후 기본값 false (자동재생 방지).
		 */
		struct Slot
		{
			std::unique_ptr<SJH::Playable::PlayableBase> playable;  ///< 보유 Playable (소유).
			bool                                         playing = false; ///< Play() 로 활성화됨 여부.
		};

		std::map<std::string, Slot> mPlayables;  ///< key -> Slot 맵 (named Playable 레지스트리).

		/// @brief @c EFacing 을 배열 인덱스로 변환.
		static int idx(Entity::EFacing f) { return static_cast<int>(f); }
		/// @brief @c EPose 를 배열 인덱스로 변환.
		static int idx(Entity::EPose p) { return static_cast<int>(p); }

		/// @brief (mFacing, mPose) 에 해당하는 그룹만 활성, 나머지 비활성 - SetActive 토글.
		void Apply();

		DirGroup        mGroups[4][2]{};                     ///< [EFacing][EPose] 8조합 레이어 그룹 테이블.
		Entity::EFacing mFacing = Entity::EFacing::Front;    ///< 현재 활성 facing (초기값 Front).
		Entity::EPose   mPose   = Entity::EPose::Idle;       ///< 현재 활성 pose (초기값 Idle).
	};
}

#endif // __TOPDOWNSHOOTER_PLAYABLE_PLAYABLE_DIRECTOR_H__
