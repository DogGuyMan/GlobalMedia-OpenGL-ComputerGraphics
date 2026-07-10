/**
 * @file iplayable.h
 * @brief @c SJH::Playable::IPlayable - 시간축 연출 순수 인터페이스.
 *
 * @details
 *  ### 책임
 *  - Client 가 단일/시퀀스/병렬 모든 시간축 연출을 동일하게 다룰 수 있는 *최소 계약* 정의.
 *  - 4-method 공개 계약 (@c Play / @c Pause / @c Stop / @c GetIsLoop) +
 *    Composite child 종료 감지용 2급 @c IsFinished.
 *  - 저장소 @c I* 인터페이스 컨벤션 준수: 모든 멤버 @c =0, protected 기본 ctor, copy/move @c delete.
 *
 *  ### 비-책임
 *  - [X] 상태 보관 (paused / finished / elapsed / isLoop) - @c PlayableBase 가 흡수.
 *  - [X] Tick(@c Update) 메서드 - @c SJH::Scene::Component::Update(float dt) 가 그 역할.
 *  - [X] @c SetIsLoop - 세팅 단계 속성이므로 @c PlayableBase 추가 public 으로 분리
 *    (spec 결정 sec.1 #5-부속).
 *
 * @note concrete 구현체는 @c PlayableBase 를 상속하고 @c OnUpdate / @c OnPlay / @c OnStop
 *       hook 만 override 한다. @c IPlayable* 핸들이 필요한 경우에만 이 인터페이스 직접 참조.
 *       정본 spec: @c doc/superpowers/specs/2026-05-26-playable-component-interface-design.md
 */
#ifndef __SJH_PLAYABLE_IPLAYABLE_H__
#define __SJH_PLAYABLE_IPLAYABLE_H__

namespace SJH::Playable
{
    /**
     * @brief 시간축 연출 순수 인터페이스 - Client 우선 4-method + @c IsFinished.
     * @details
     *  - 저장소 @c I* 컨벤션 준수: pure interface, 모든 멤버 @c =0, protected ctor + copy/move @c delete.
     *  - 실제 상태/디폴트 구현은 @c PlayableBase (다중 상속 abstract base) 가 흡수.
     *  - Tick 메서드 없음 - @c SJH::Scene::Component::Update(float dt) 가 그 역할
     *    (@c PlayableBase 의 @c Update @c final 에서 @c OnUpdate hook 으로 디스패치).
     *
     *  ### 결정: Stop vs Pause 의미 구분 (spec sec.1 결정 #1)
     *  - @c Pause: 상태 보존, @c Play 로 재개 - 중단 위치부터 계속.
     *  - @c Stop: 리셋 후 정지 - 다음 @c Play 시 첫 프레임부터 (Unity @c AudioSource.Stop 정통).
     *
     *  ### 결정: Loop 자동 재시작 (spec sec.1 결정 #2)
     *  - @c GetIsLoop() == true 이면 내부에서 자동 재시작 -> @c IsFinished() 절대 @c true 안 됨.
     *  - 외부 종료는 @c Stop() 호출 전까지 재생 계속.
     */
    class IPlayable
    {
      protected:
        IPlayable() = default;

      public:
        virtual ~IPlayable() = default;
        IPlayable(const IPlayable&)            = delete;
        IPlayable& operator=(const IPlayable&) = delete;
        IPlayable(IPlayable&&)                 = delete;
        IPlayable& operator=(IPlayable&&)      = delete;

        // === Client 우선 4-method ===

        /// @brief 재생 시작. Pause 후 재개, Stop 후 첫 프레임부터 시작 모두 동일 진입점.
        virtual void Play()  = 0;

        /// @brief 일시정지. 내부 상태 보존 - 다음 @c Play() 로 중단 위치에서 재개.
        virtual void Pause() = 0;

        /// @brief 리셋 후 정지. elapsed/cursor 초기화 - 다음 @c Play() 는 첫 프레임.
        virtual void Stop()  = 0;

        /// @brief 루프 여부. @c true 면 종료 시 자동 재시작 -> @c IsFinished() 절대 @c true 안 됨.
        virtual bool GetIsLoop() const = 0;

        // === 2급 공개 - Composite 의 child 종료 감지 + 외부 despawn 결정 ===

        /// @brief 재생 종료 여부. Loop=true 면 절대 @c true 반환 안 함.
        /// @details @c SequencePlayable 이 현재 child 가 끝났음을 감지해 다음 child @c Play() 를 트리거,
        ///          @c ParallelPlayable 이 전 children 종료 합산, 혹은 외부가 Bullet despawn 결정 등에 사용.
        virtual bool IsFinished() const = 0;
    };
}

#endif // __SJH_PLAYABLE_IPLAYABLE_H__
