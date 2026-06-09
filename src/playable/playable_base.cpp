/**
 * @file playable_base.cpp
 * @brief @c SJH::Playable::PlayableBase out-of-line dtor 정의.
 *
 * @details
 *  ### 책임
 *  - @c PlayableBase 다중 상속 vtable anchor - 이 단일 컴파일 단위에 vtable 이 emit 되어 ODR 안정.
 *
 *  ### 비-책임
 *  - [X] @c Play / @c Pause / @c Stop / @c Update 로직 - 헤더 inline 에서 처리.
 *  - [X] 연출 로직 - concrete 파생 클래스의 @c OnUpdate 에서 담당.
 *
 * @note dtor out-of-line 패턴: 다중 상속 클래스는 헤더 inline dtor 로 두면 여러 번역 단위에
 *       vtable duplicate 가 생겨 링크 경고 또는 ODR 위반 가능. @c =default 를 .cpp 에 두면
 *       vtable 이 단일 TU 에 집중됨.
 */
#include "playable/playable_base.h"

namespace SJH::Playable
{
    // out-of-line dtor - 다중 상속 vtable anchor.
    // 이 단일 컴파일 단위에 vtable 이 emit 되어 ODR 안정.
    PlayableBase::~PlayableBase() = default;
}
