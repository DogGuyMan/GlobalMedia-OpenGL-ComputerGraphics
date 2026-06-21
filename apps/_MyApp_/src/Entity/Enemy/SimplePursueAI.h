/**
 * @file SimplePursueAI.h
 * @brief 플레이어를 향해 직선 추적하는 최소 적 AI Component.
 *
 * @details
 *  ### 책임
 *  - 매 프레임 타깃(플레이어) 방향으로 정규화한 속도 벡터를 적 body 에 적용.
 *  - 사망/넉백 상태에 따라 추적을 정지하거나 일시 양보.
 *
 *  ### 비-책임
 *  - [X] 사망 비활성/despawn - Life 의 사망 지연(mDieTimer)이 담당 (dissolve 연출 시간 확보).
 *  - [X] 경로 탐색/장애물 회피 - 직선 추적만 (이름 그대로 simple).
 *
 *  ### 정통 매핑
 *  - 게임 AI 의 가장 단순한 seek behavior - 타깃 방향 단위벡터 x 속도.
 *
 * @note XZ 월드 평면을 Box2D XY 로 사상한다 (Z -> -Y, spec 4.4).
 */
#ifndef __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
#define __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__

#include "scene/actor.h"
#include <glm/glm.hpp>

class b2Body;

namespace TopdownShooter::Entity { class BaseEntity; }

namespace TopdownShooter::Entity::Enemy
{
    /**
     * @brief 매 프레임 Player 위치를 향해 SetLinearVelocity 하는 추적 AI.
     * @details
     *  사망 시에는 추적을 멈추고 속도를 0 으로만 만든다 (비활성/despawn 은 Life 의 사망 지연이 담당).
     *  넉백(impulse) 중에는 속도 설정을 건너뛰어 버스트를 보존하고, 종료 후 자동으로 추적을 재개한다.
     */
    class SimplePursueAI : public SJH::Scene::Component
    {
      public:
        /// @brief 추적 대상/물리 body/속도를 주입.
        /// @param target 추적할 Actor (플레이어). 위치만 매 프레임 읽는다 (비소유).
        /// @param body   속도를 적용할 적 Box2D body (비소유).
        /// @param speed  추적 이동 속도 (단위/초).
        SimplePursueAI(SJH::Scene::Actor* target, b2Body* body, float speed);
        ~SimplePursueAI() override;

        /// @brief owner 의 BaseEntity facade 를 캐시 (IsAlive/IsImpulseActive 매 프레임 조회용).
        void OnEnter() override;
        void OnExit()  override {}
        /// @brief 사망/넉백 게이트를 거쳐 타깃 방향 속도를 적용. @p dt 미사용 (속도 직접 설정).
        void Update(float dt) override;

      private:
        SJH::Scene::Actor* mTarget;  ///< 추적 대상 (플레이어) - 위치 읽기 전용, 비소유.
        b2Body*            mBody;    ///< 속도를 적용할 적 Box2D body, 비소유.
        float              mSpeed;   ///< 추적 이동 속도 (단위/초).
        TopdownShooter::Entity::BaseEntity* mEntity = nullptr; ///< owner facade 캐시 (IsAlive/IsImpulseActive).
    };
}

#endif // __TOPDOWNSHOOTER_ENTITY_ENEMY_SIMPLE_PURSUE_AI_H__
