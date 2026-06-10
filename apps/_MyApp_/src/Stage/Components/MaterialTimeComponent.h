/**
 * @file MaterialTimeComponent.h
 * @brief 부착된 Material 의 "uTime" 프로퍼티를 누적 dt 로 갱신하는 Component.
 *
 * @details
 *  ### 책임
 *  - Actor 의 @c Update(dt) 경로에서 @c mElapsed 를 누적하여 @c Material::Properties.Floats["uTime"]
 *    에 실시간으로 기록한다.
 *  - transparent.vs 의 U 스크롤(PoliceTape 흐름 등) 셰이더에 시간을 공급한다.
 *
 *  ### 비-책임
 *  - [X] 렌더링 직접 수행 — Material 값 갱신만 담당. 렌더는 @c MeshPassProcessor 가 담당.
 *  - [X] 전역 시간 공급 — 스카이박스의 @c u_time 은 @c render() 루프가 직접 송신.
 *
 * @note 벽마다 Material 인스턴스가 다르므로 per-Actor 부착 패턴을 사용한다.
 *       단일 Material 을 여러 Actor 가 공유할 경우 마지막 Update 가 덮어쓴다.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__

#include "material/material.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /**
     * @brief 부착된 Material 의 "uTime" 프로퍼티를 누적 dt 로 매 프레임 갱신하는 Component.
     * @details
     *  transparent.vs 의 시간 기반 U 스크롤(흐르는 PoliceTape) 을 위한 시간 공급원.
     *  벽마다 material instance 가 달라 per-actor 부착 — 인스턴스가 자기 시간(@c mElapsed) 을 보유.
     *  스카이박스 @c u_time (@c render() 루프 직접 송신) 과 달리 @c Director::Update(dt) 경로로 자기완결.
     */
    class MaterialTime : public SJH::Scene::Component
    {
    public:
        /// @brief 생성자 — 갱신 대상 Material 을 주입.
        /// @param material "uTime" 프로퍼티를 보유한 Material 포인터 (비소유). nullptr 이면 Update 는 no-op.
        explicit MaterialTime(SJH::Material* material) : mMaterial(material) {}

        void OnEnter() override {}
        void OnExit() override {}
        void Update(float dt) override
        {
            mElapsed += dt;
            if (mMaterial)
                mMaterial->Properties.Floats["uTime"] = mElapsed;
        }

    private:
        SJH::Material* mMaterial = nullptr; ///< 갱신 대상 Material (비소유). nullptr 이면 Update 는 no-op.
        float mElapsed = 0.0f;              ///< 생성 이후 누적 시간(초) — Material "uTime" 에 기록.
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__
