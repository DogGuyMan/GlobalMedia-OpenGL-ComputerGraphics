#ifndef __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__
#define __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__

#include "material/material.h"
#include "scene/actor.h"

namespace TopdownShooter::Stage::Components
{
    /// @brief 부착된 Material 의 "uTime" 프로퍼티를 누적 dt 로 매 프레임 갱신.
    /// @details
    ///   - transparent.vs 의 시간 기반 U 스크롤(흐르는 PoliceTape)용 시간 공급원.
    ///   - 벽마다 material instance 가 달라 per-actor 부착 — 인스턴스가 자기 시간 보유.
    ///   - 스카이박스 u_time(render() 루프) 과 달리 Director::Update(dt) 경로로 자기완결.
    class MaterialTime : public SJH::Scene::Component
    {
    public:
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
        SJH::Material* mMaterial = nullptr;
        float mElapsed = 0.0f;
    };
}

#endif // __TOPDOWNSHOOTER_STAGE_COMPONENTS_MATERIAL_TIME_H__
