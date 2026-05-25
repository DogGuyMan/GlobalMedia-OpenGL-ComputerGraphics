#include "playable/playable_base.h"

namespace SJH::Playable
{
    // out-of-line dtor — 다중 상속 vtable anchor.
    // 이 단일 컴파일 단위에 vtable 이 emit 되어 ODR 안정.
    PlayableBase::~PlayableBase() = default;
}
