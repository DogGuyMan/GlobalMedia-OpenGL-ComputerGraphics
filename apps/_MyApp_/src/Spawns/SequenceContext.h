#ifndef __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
#define __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__

// fwd — 모두 포인터 보유라 전방 선언으로 충분
namespace SJH { class ResourceRegistry; }
namespace SJH::Scene { class Actor; }
namespace TopdownShooter::Audio { class AudioSystem; }
namespace TopdownShooter::VFX   { class VFXSystem; }
class b2World;

namespace TopdownShooter::Spawns
{
    /// @brief 시퀀스 빌더가 필요로 하는 의존 묶음 (명시적 주입 — Q2).
    ///        Manager::Get() + ResourceRegistry::Get() 으로 호출 측이 1회 조립.
    struct SequenceContext
    {
        Audio::AudioSystem*  audio    = nullptr;
        VFX::VFXSystem*      vfx      = nullptr;
        SJH::ResourceRegistry* reg    = nullptr;
        b2World*             world    = nullptr;  // bullet spawn 용
        SJH::Scene::Actor*   sceneRoot = nullptr; // bullet/enemy 부모
        SJH::Scene::Actor*   fxRoot   = nullptr;  // 단발 FX 부모 (sweep 대상)
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
