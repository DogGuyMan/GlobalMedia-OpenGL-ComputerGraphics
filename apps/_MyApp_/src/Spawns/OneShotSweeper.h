/**
 * @file OneShotSweeper.h
 * @brief 단발 FX Actor 완료 후 deferred 파괴 - 프레임 끝 fxRoot 청소기.
 *
 * @details
 *  ### 책임
 *  - @c AutoDespawnOnFinish::IsDone() 이 true 인 자식 Actor 를 @c fxRoot 에서 @c RemoveChild.
 *  - 매 프레임 씬 Update 직후 1회 호출 - Step/Update 순회 중 반복자 무효화를 피하는 deferred 방식.
 *  ### 비-책임
 *  - [X] IsDone 판정 로직 - @c AutoDespawnOnFinish 컴포넌트가 담당.
 *  - [X] 호출 타이밍 스케줄링 - 호출자(main 루프 또는 Stage) 가 Update 직후 직접 호출.
 *  - [X] 비-FX 자식(BGM Actor 등) 파괴 - @c AutoDespawnOnFinish 미부착 Actor 는 스캔 대상 아님.
 *  ### 정통 매핑
 *  - Cocos2D end-of-frame @c Node::_childrenIndexDirty 클린업 + @c PoolManager::clear - 프레임 후 일괄 제거.
 *  - Unity @c Object.Destroy(obj) 지연 실행 - Update 이후 실제 파괴.
 *
 * @note @c FindChildIf 는 매 호출 새 스캔이므로 @c RemoveChild 직후 반복자 무효화가 없다.
 *       완료 Actor 가 0개이면 while 루프 진입 없이 즉시 반환 (비용 최소).
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
#define __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__

namespace SJH::Scene { class Actor; }

namespace TopdownShooter::Spawns
{
    /// @brief fxParent 자식 중 @c AutoDespawnOnFinish::IsDone() 이 true 인 child 를 RemoveChild 로 파괴.
    /// @details 매 프레임 씬 Update 직후 1회 호출 (Cocos end-of-frame cleanup 정통).
    ///          @c RemoveChild 는 @c OnExit 후 erase - leaf dtor 가 FMOD/Effekseer 자원을 정리한다.
    /// @param fxParent 단발 FX Actor 들의 부모 (sweep 대상 트리 루트).
    void SweepFinishedChildren(SJH::Scene::Actor& fxParent);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_ONE_SHOT_SWEEPER_H__
