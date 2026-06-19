/**
 * @file EnemyBuilder.h
 * @brief 적 1체를 조립해 씬 트리에 부착하는 부트스트랩 빌더 자유 함수 + 입력 의존 struct.
 *
 * @details
 *  ### 책임
 *  - @c EnemyDeps (비싱글턴 의존만 묶은 PoD) 로 적 1체 조립 명세 수신.
 *  - @c EnemyFactory + 스프라이트/애니 + EntityPresentation + 데칼 + spawnParent 부착을 조율.
 *
 *  ### 비-책임([X])
 *  - [X] 물리 body 생성 - @c Entity::Enemy::CreateEnemyActor (factory) 위임.
 *  - [X] 적 스폰 타이밍/웨이브 관리 - @c WaveController 가 호출 주체.
 *
 *  ### 정통 매핑
 *  - Unity 의 prefab + spawner - struct deps = 인스펙터 노출 필드, BuildEnemy = Instantiate.
 *
 * @note PlayerBuilder 와 대칭 (Deps struct -> Build 자유 함수). 적은 결과 struct 없이 Actor* 직접 반환.
 */
#ifndef __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
#define __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__

#include "Entity/Constants.h"
#include <vmath.h>

class b2World;
namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Bootstrap
{
    /// @brief BuildEnemy 입력 의존 (PlayerDeps 미러 - 비싱글턴만).
    /// @details 싱글톤(ResourceRegistry/Director/GameSystems)은 BuildEnemy 내부에서 ::Get() 조회.
    ///          여기 묶이는 것은 호출자(WaveController)만 아는 비싱글턴 값/포인터.
    struct EnemyDeps
    {
        b2World*           world        = nullptr;   ///< 적 body 를 생성할 Box2D 월드 (factory 로 전달)
        SJH::Scene::Actor* spawnParent  = nullptr;   ///< 적 child 부착 부모 (WaveController.mSpawnParent)
        SJH::Scene::Actor* playerTarget = nullptr;   ///< SimplePursueAI 추적 대상
        vmath::vec2        pos          = vmath::vec2(0.0f);  ///< 스폰 월드 좌표 (xy 평면)
        int                hp           = Entity::ENEMY_HP;       ///< 초기 체력
        float              speed        = Entity::ENEMY_SPEED;    ///< 이동 속도
        int                damage       = Entity::ENEMY_DAMAGE;   ///< 접촉 데미지
        int                variant      = 0;          ///< 0~2 -> ENEMY_FRONT[variant % 3]
        float              spriteFps    = Entity::ENEMY_SPRITE_FPS;        ///< 2프레임 walk 애니 속도
        vmath::vec4        healthBarColor = vmath::vec4(1.0f, 0.15f, 0.12f, 1.0f); ///< 머리 위 체력바 채움 색 (기본 빨강 - 적 베리에이션)
    };

    /// @brief 적 1체 조립 - CreateEnemyActor + ENEMY_FRONT 스프라이트/애니 + spawnParent 부착.
    /// @details 조립 순서: (1) EnemyFactory 로 물리+Life+AI+contact 코어 생성 ->
    ///          (2) renderActor 에 스프라이트 레이어 + 상시 펄스/워블 트윈(ParallelPlayable) ->
    ///          (3) EntityPresentation 공통 연출(director/hit/death/체력바) + 적 hit 에 Damaged 사운드 ->
    ///          (4) hit FX/데미지 숫자 seam 주입 + 발밑 데칼 -> (5) spawnParent 부착(OnEnter 캐스케이드).
    /// @param deps 적 조립 명세 (월드/부모/타깃/스탯/베리에이션).
    /// @return 트리 부착된 적 Actor* (@p deps.spawnParent 가 nullptr 이면 nullptr).
    SJH::Scene::Actor* BuildEnemy(const EnemyDeps& deps);
}

#endif // __TOPDOWNSHOOTER_BOOTSTRAP_ENEMY_BUILDER_H__
