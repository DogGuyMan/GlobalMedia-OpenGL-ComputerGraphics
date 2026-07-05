/**
 * @file VfxFacade.cpp
 * @brief @c TopdownShooter::VFX 전역 스폰 파사드 구현 -- 전역 컨텍스트(gFxRoot, gVfx) 보관 + Spawn 위임.
 *
 * @details
 *  ### 책임
 *  - @c SetSpawnContext 로 main 이 1회 등록한 @c gFxRoot / @c gVfx 를 정적 변수로 보관.
 *  - @c Spawn(key, pos, yaw) 호출 시 @c ResourceRegistry::Get().FindEffect(key) 조회 후
 *    @c Spawns::SpawnVfxInstance 로 위임.
 *  - @c GetSpawnFxRoot / @c GetSpawnVfx 로 합성 VFX 가 컨텍스트를 직접 얻을 수 있도록 노출.
 *  ### 비-책임
 *  - [X] EffekseerPlayable / Actor 조립 -- @c Spawns::SpawnVfxInstance 담당.
 *  - [X] Effect 에셋 로드 -- @c ResourceRegistry 담당.
 *
 * @note @c VfxInstance.cpp 와 TU 분리 필수 -- 본 파일은 @c resource_registry.h(-> @c gl3w.h) 만 포함.
 *       @c VfxInstance.cpp 는 @c VFXSystem.h(-> @c EffekseerRendererGL) 를 끌어와
 *       @c PFNGL* 재정의 충돌 유발 가능. 파사드는 @c VFXSystem* 를 포인터로만 다뤄 완전형 불필요.
 */
#include <GL/gl3w.h> // 반드시 최상단 - resource_registry.h->render_texture.h->gl3w.h 보다 먼저.

// VFX 단발 스폰 파사드 정의 (선언은 Spawns/VfxInstance.h 의 TopdownShooter::VFX namespace).
// - VfxInstance.cpp 와 분리한 이유: 파사드는 resource_registry.h(->gl3w) 만 필요하고,
//   VfxInstance.cpp 는 VFXSystem.h(->EffekseerRendererGL 의 시스템 gl3.h) 를 끌어온다.
//   둘을 한 TU 에 두면 gl3w <-> 시스템 gl3.h 가 충돌(PFNGL* 재정의)하므로 TU 를 가른다.
//   파사드는 VFXSystem* 을 *포인터* 로만 다뤄 완전형(EffekseerRendererGL)이 불필요.

#include "Spawns/VfxInstance.h"                     // VFX::Spawn/SetSpawnContext 선언 + Spawns::SpawnVfxInstance
#include "resource_registry/resource_registry.h"   // SJH::ResourceRegistry::Get().FindEffect

#include <glm/glm.hpp> // glm::vec3 (PendingVfx 멤버 -- 직접 의존 명시)
#include <string>
#include <vector>

namespace TopdownShooter::VFX
{
    namespace
    {
        SJH::Scene::Actor* gFxRoot = nullptr; ///< main 등록 -- Director.Root() 자식 "FxRoot".
        VFXSystem*         gVfx    = nullptr; ///< main 등록 -- VFXSystem 포인터.

        /// @brief 지연 스폰 요청 1건 -- Spawn 이 적재, FlushSpawns 가 Director::Update 밖에서 소비.
        struct PendingVfx
        {
            std::string key; ///< ResourceRegistry Effect 키 (값 보유 -- 호출측 수명 무관).
            glm::vec3 pos; ///< 월드 스폰 위치 (데미지 시점 캡처).
            float       yaw; ///< Y 축 회전(라디안).
        };
        std::vector<PendingVfx> gPending; ///< 이번 프레임 누적 스폰 요청. FlushSpawns 가 drain.
    }

    void SetSpawnContext(SJH::Scene::Actor* fxRoot, VFXSystem* vfx)
    {
        gFxRoot = fxRoot;
        gVfx    = vfx;
    }

    void Spawn(const char* key, const glm::vec3& pos, float yaw)
    {
        if (gFxRoot == nullptr || gVfx == nullptr) return;     // 미등록 - no-op
        // [!] 즉시 AddChild 금지 -- Spawn 은 Life seam 경유로 Director::Update 순회 *도중*
        //     (UltimateLaser 의 RaycastAll->DoDamaged) 호출될 수 있다. 순회 중 fxRoot->AddChild 는
        //     mChildren 재할당 -> 라이브 iterator 무효화(use-after-free) 유발. 요청만 적재하고
        //     실제 AddChild 는 FlushSpawns(Director::Update 밖)가 수행한다 (SweepDespawned 대칭).
        gPending.push_back(PendingVfx{key, pos, yaw});
    }

    void FlushSpawns()
    {
        if (gPending.empty()) return;
        // 재진입 안전 -- flush 도중 새 Spawn 요청은 다음 프레임 batch 로 (gPending 비운 뒤 소비).
        std::vector<PendingVfx> batch;
        batch.swap(gPending);
        if (gFxRoot == nullptr || gVfx == nullptr) return;     // 컨텍스트 해제됨 - 잔여 요청 폐기
        for (const auto& p : batch)
        {
            SJH::Effect* fx = SJH::ResourceRegistry::Get().FindEffect(p.key.c_str());
            Spawns::SpawnVfxInstance(*gFxRoot, gVfx, fx, p.pos, p.yaw); // fx nullptr 면 내부 guard
        }
    }

    SJH::Scene::Actor* GetSpawnFxRoot() { return gFxRoot; }
    VFXSystem*         GetSpawnVfx()    { return gVfx; }
}
