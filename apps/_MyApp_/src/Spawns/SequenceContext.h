/**
 * @file SequenceContext.h
 * @brief 시퀀스 빌더 함수군이 공유하는 의존성 묶음 구조체.
 *
 * @details
 *  ### 책임
 *  - 각 시퀀스 빌더(@c BuildBGM / @c SpawnHitSpark 등)가 필요한 서브시스템 포인터를 한 곳에 모아
 *    명시적 주입(Q2 - DI without container) 패턴으로 전달한다.
 *  - 호출 측이 @c Manager::Get() + @c ResourceRegistry::Get() 으로 1회 조립 후 재사용.
 *  ### 비-책임
 *  - [X] 서브시스템 수명 관리 - 각 포인터 원소의 수명은 호출자(App/Scene) 가 보장.
 *  - [X] 필드 유효성 보장 - 각 빌더 함수가 nullptr 가드를 스스로 수행.
 *  ### 정통 매핑
 *  - Unreal @c FActorSpawnParameters - 스폰 시 의존 묶음을 인라인 구조체로 전달하는 패턴.
 *
 * @note 모든 필드 기본값은 nullptr. 사용하지 않는 서브시스템은 nullptr 로 두면 해당 기능이 no-op 처리된다.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
#define __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__

// fwd - 모두 포인터 보유라 전방 선언으로 충분
namespace SJH { class ResourceRegistry; }
namespace SJH::Scene { class Actor; }
namespace TopdownShooter::Audio { class AudioSystem; }
namespace TopdownShooter::VFX   { class VFXSystem; }
class b2World;

namespace TopdownShooter::Spawns
{
    /**
     * @brief 시퀀스 빌더가 필요로 하는 의존 묶음 (명시적 주입 - Q2).
     * @details 호출 측이 @c Manager::Get() + @c ResourceRegistry::Get() 으로 1회 조립 후
     *          각 빌더 함수에 const ref 로 전달한다.
     *          각 필드는 nullptr 허용 - 빌더 함수가 내부에서 nullptr 가드 후 no-op 처리.
     */
    struct SequenceContext
    {
        Audio::AudioSystem*    audio     = nullptr;  ///< FMOD Studio 이벤트 로드/재생. nullptr 이면 오디오 스폰 skip.
        VFX::VFXSystem*        vfx       = nullptr;  ///< Effekseer 파티클 관리자. nullptr 이면 VFX 스폰 skip.
        SJH::ResourceRegistry* reg       = nullptr;  ///< Effect/Texture 리소스 조회. nullptr 이면 VFX 스폰 skip.
        b2World*               world     = nullptr;  ///< Box2D 물리 월드 (bullet spawn 레이캐스트 용).
        SJH::Scene::Actor*     sceneRoot = nullptr;  ///< bullet/enemy 등 게임 오브젝트의 부모 Actor.
        SJH::Scene::Actor*     fxRoot    = nullptr;  ///< 단발 FX Actor 부모 (sweep 대상 트리 루트).
    };
}

#endif // __TOPDOWNSHOOTER_SPAWNS_SEQUENCE_CONTEXT_H__
