#ifndef __MYAPP_PHYSICS_FILTER_H__
#define __MYAPP_PHYSICS_FILTER_H__

#include <cstdint>

namespace TopdownShooter::Physics::Filter
{
    // b2.4.1 categoryBits / maskBits 는 uint16. 6비트면 무난 (spec §4.3).
    constexpr uint16_t PLAYER        = 1u << 0;
    constexpr uint16_t ENEMY         = 1u << 1;
    constexpr uint16_t BULLET_PLAYER = 1u << 2;
    constexpr uint16_t BULLET_ENEMY  = 1u << 3;
    constexpr uint16_t WALL          = 1u << 4;
    constexpr uint16_t PICKUP        = 1u << 5;

    // 자주 쓰는 mask 조합
    constexpr uint16_t PLAYER_MASK = ENEMY | BULLET_ENEMY | WALL | PICKUP;
    constexpr uint16_t ENEMY_MASK  = PLAYER | BULLET_PLAYER | WALL;
    constexpr uint16_t WALL_MASK   = PLAYER | ENEMY | BULLET_PLAYER | BULLET_ENEMY;
}

#endif // __MYAPP_PHYSICS_FILTER_H__
