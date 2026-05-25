#ifndef __MYAPP_PHYSICS_FILTER_H__
#define __MYAPP_PHYSICS_FILTER_H__

#include <cstdint>

namespace TopdownShooter::Physics
{
    /// @brief Unity LayerMask 정통 — bitmask 충돌 필터 layer.
    /// @details
    ///   ### 폭 (uint64_t)
    ///   미래 확장 여유 64-bit. Box2D 2.4.1 의 `b2Filter::categoryBits/maskBits` 는
    ///   uint16 이므로 사용 시점에 `ToBits()` 로 하위 16 비트 narrowing 필요.
    ///   현 사용 범위 (bit 0~5) 는 16비트 안에 안전히 들어옴.
    ///
    ///   ### bitwise 연산
    ///   `operator| / & / ~` 를 정의하여 `PhysicsLayer::Player | PhysicsLayer::Wall`
    ///   같은 자연 표기 가능 (enum class 의 강한 타입 보존).
    enum class PhysicsLayer : uint64_t
    {
        None         = 0,
        Player       = 1ull << 0,
        Enemy        = 1ull << 1,
        BulletPlayer = 1ull << 2,
        BulletEnemy  = 1ull << 3,
        Wall         = 1ull << 4,
        Pickup       = 1ull << 5,
    };

    constexpr PhysicsLayer operator|(PhysicsLayer a, PhysicsLayer b)
    {
        return static_cast<PhysicsLayer>(
            static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    constexpr PhysicsLayer operator&(PhysicsLayer a, PhysicsLayer b)
    {
        return static_cast<PhysicsLayer>(
            static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
    }

    constexpr PhysicsLayer operator~(PhysicsLayer a)
    {
        return static_cast<PhysicsLayer>(~static_cast<uint64_t>(a));
    }

    /// @brief Box2D 2.4.1 `b2Filter::categoryBits/maskBits` (uint16) 어댑터.
    constexpr uint16_t ToBits(PhysicsLayer l)
    {
        return static_cast<uint16_t>(static_cast<uint64_t>(l));
    }

    // 자주 쓰는 mask 조합 — "X 가 부딪힐 대상" 의미.
    constexpr PhysicsLayer PlayerMask = PhysicsLayer::Enemy | PhysicsLayer::BulletEnemy | PhysicsLayer::Wall | PhysicsLayer::Pickup;
    constexpr PhysicsLayer EnemyMask  = PhysicsLayer::Player | PhysicsLayer::BulletPlayer | PhysicsLayer::Wall;
    constexpr PhysicsLayer WallMask   = PhysicsLayer::Player | PhysicsLayer::Enemy | PhysicsLayer::BulletPlayer | PhysicsLayer::BulletEnemy;
}

#endif // __MYAPP_PHYSICS_FILTER_H__
