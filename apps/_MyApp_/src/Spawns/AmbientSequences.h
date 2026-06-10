/**
 * @file AmbientSequences.h
 * @brief 배경 환경음 시퀀스 빌더 — BGM Actor 를 sceneRoot 에 조립한다.
 *
 * @details
 *  ### 책임
 *  - BGM 전용 @c SJH::Scene::Actor ("BgmActor") 를 @c SequenceContext::sceneRoot 아래 스폰.
 *  - @c FmodStudioPlayable(event:/BGM) 을 loop 모드로 부착해 Stage FSM 이 Play 를 제어하도록 준비.
 *  ### 비-책임
 *  - [X] Play() 호출 — @c TitleState::OnEnter 가 소유 (startup 이중 재생 방지).
 *  - [X] 단발 FX 관리 — @c CombatSequences / @c OneShotSweeper 담당.
 *  - [X] AudioSystem / SceneRoot 수명 — 호출자(Builder 또는 Stage) 가 보장.
 *  ### 정통 매핑
 *  - Cocos2D @c SimpleAudioEngine::playBackgroundMusic — 씬 진입 시 BGM 로드 후 FSM 이 재생 트리거.
 *
 * @note @c SequenceContext::audio 또는 @c sceneRoot 가 nullptr 이면 no-op 으로 안전하게 반환.
 */
#ifndef __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
#define __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__

#include "Spawns/SequenceContext.h"

namespace TopdownShooter::Spawns
{
    /// @brief BGM — sceneRoot 밑 "BgmActor" 에 FmodStudioPlayable(event:/BGM) loop 무한 부착.
    /// @details 단발(one-shot) 아님 — 지속 재생이라 fxRoot/AudioInstance 대신 sceneRoot 직접 부착 + SetIsLoop(true).
    ///          Play() 는 여기서 호출하지 않는다 — @c TitleState::OnEnter 가 재생 시작을 소유.
    /// @param ctx 의존 묶음. @c ctx.audio 와 @c ctx.sceneRoot 가 유효해야 실행된다.
    void BuildBGM(const SequenceContext& ctx);
}

#endif // __TOPDOWNSHOOTER_SPAWNS_AMBIENT_SEQUENCES_H__
