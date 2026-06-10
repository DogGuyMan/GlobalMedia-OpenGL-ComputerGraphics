/**
 * @file StageBuilder.h
 * @brief 스테이지 조립 자유 함수 @c CreateStageActor 선언.
 *
 * @details
 *  ### 책임
 *  - @c StageConfig 를 받아 물리 벽 4개 + PCB 모델 + Orbit VFX + @c StageState Component 를
 *    자식/컴포넌트로 조립한 Stage @c Actor 를 생성·반환.
 *  - 필요한 공유 자원(Mesh / Program / Texture / Material / Model)을 @c cfg.registry 에
 *    idempotent 하게 등록(이미 있으면 @c Find 재사용).
 *
 *  ### 비-책임
 *  - [X] Actor 트리 편입 — 호출자(@c main.cpp startup) 가 반환된 Actor 를
 *        @c Director::Root().AddChild 로 위탁.
 *  - [X] WaveController 생성 / Player 배선 — 호출자 책임.
 *  - [X] world / registry lifetime 소유 — 호출자 소유, 본 빌더는 비소유 포인터 수신.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c Scene::createWithPhysics() + @c PhysicsBody 조립 패턴.
 *  - Unreal @c AGameMode::InitGame + @c SpawnActor 조합에 대응.
 *
 * @note @c cfg.world 또는 @c cfg.registry 가 @c nullptr 이면 내부에서 @c assert 발동.
 *       배경 VFX(@c orbital_background.efk) 는 ResourceRegistry 에 등록된 경우에만 생성 — 없으면 no-op.
 */
#ifndef __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
#define __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__

#include "Stage/StageConfig.h"
#include "scene/actor.h"
#include <memory>

namespace TopdownShooter::Stage
{
    /// @brief Stage Actor 생성 — walls + pickups + StageState Component 가 child/component 로 매단 일반 Actor 반환.
    /// @details
    ///   - plane mesh / wallMat / pickupMat / simple.vs/fs Program / pcb model 은 cfg.registry 에 자동 등록
    ///     (key: "stage_plane" / "stage_wall" / "stage_pickup" / "stage_solid_plane" / "stage_pcb").
    ///   - 같은 key 가 이미 있으면 Find 로 재사용 (idempotent).
    ///   - cfg.world 또는 cfg.registry 가 nullptr 이면 assert.
    /// @param cfg StageConfig — world + registry 필수.
    /// @return Stage Actor (호출자가 Director::Root().AddChild 로 위탁)
    std::unique_ptr<SJH::Scene::Actor> CreateStageActor(const StageConfig& cfg);
}

#endif // __TOPDOWNSHOOTER_STAGE_STAGE_BUILDER_H__
