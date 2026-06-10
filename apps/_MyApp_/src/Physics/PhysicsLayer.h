/**
 * @file PhysicsLayer.h
 * @brief Box2D 충돌 필터 레이어 bitmask -- Unity LayerMask 정통 매핑.
 *
 * @details
 *  ### 책임
 *  - @c PhysicsLayer enum class (uint64_t) 로 레이어 비트를 정의.
 *  - @c operator| / & / ~ 연산자 제공 -> 자연 표기 조합 가능
 *    (예: @c PhysicsLayer::Player | PhysicsLayer::Wall).
 *  - @c ToBits(PhysicsLayer): Box2D 2.4.1 @c b2Filter::categoryBits/maskBits (uint16_t) 어댑터.
 *  - 자주 쓰는 mask 조합 상수 (@c PlayerMask / @c EnemyMask / @c WallMask).
 *
 *  ### 비-책임
 *  - [X] b2Filter 직접 설정 -- @c BodyConfig::categoryBits/maskBits + @c ToBits 변환으로 주입.
 *
 *  ### 정통 매핑
 *  - Unity @c LayerMask (int bitmask) + @c Physics2D.GetLayerCollisionMask 개념.
 *
 * @note uint64_t 로 선언하나 Box2D 2.4.1 의 @c b2Filter 는 uint16 만 지원한다.
 *       @c ToBits() 호출 시 하위 16 비트로 narrowing 됨. 현 사용 범위(bit 0~5)는 안전.
 */
#ifndef __MYAPP_PHYSICS_FILTER_H__
#define __MYAPP_PHYSICS_FILTER_H__

#include <cstdint>

namespace TopdownShooter::Physics
{
    /**
     * @brief Unity LayerMask 정통 -- bitmask 충돌 필터 레이어 열거형.
     * @details
     *  64-bit 폭으로 미래 확장 여유를 확보하나, Box2D 2.4.1 @c b2Filter 는 uint16 만 지원한다.
     *  사용 시점에 @c ToBits() 로 하위 16 비트 narrowing 필요.
     *  현 사용 범위 (bit 0~5) 는 16비트 안에 안전히 들어옴.
     *
     *  @c operator| / & / ~ 를 정의해 강한 타입을 유지하면서 자연 표기 조합이 가능하다
     *  (예: @c PhysicsLayer::Player | PhysicsLayer::Wall).
     */
    enum class PhysicsLayer : uint64_t
    {
        None         = 0,           ///< 충돌 없음 (필터 비트 0).
        Player       = 1ull << 0,   ///< 플레이어 레이어 (bit 0).
        Enemy        = 1ull << 1,   ///< 적 레이어 (bit 1).
        BulletPlayer = 1ull << 2,   ///< 플레이어 발사체 레이어 (bit 2).
        BulletEnemy  = 1ull << 3,   ///< 적 발사체 레이어 (bit 3).
        Wall         = 1ull << 4,   ///< 벽(정적 장애물) 레이어 (bit 4).
        Pickup       = 1ull << 5,   ///< 아이템 픽업 레이어 (bit 5).
    };

    /// @brief 비트 OR 조합. @c enum class 강한 타입 보존.
    constexpr PhysicsLayer operator|(PhysicsLayer a, PhysicsLayer b)
    {
        return static_cast<PhysicsLayer>(
            static_cast<uint64_t>(a) | static_cast<uint64_t>(b));
    }

    /// @brief 비트 AND 교집합. 두 레이어 간 공통 비트 추출.
    constexpr PhysicsLayer operator&(PhysicsLayer a, PhysicsLayer b)
    {
        return static_cast<PhysicsLayer>(
            static_cast<uint64_t>(a) & static_cast<uint64_t>(b));
    }

    /// @brief 비트 NOT. 특정 레이어를 mask 에서 제외할 때 사용.
    constexpr PhysicsLayer operator~(PhysicsLayer a)
    {
        return static_cast<PhysicsLayer>(~static_cast<uint64_t>(a));
    }

    /// @brief Box2D 2.4.1 @c b2Filter::categoryBits/maskBits (uint16_t) 어댑터.
    /// @param l 변환할 PhysicsLayer 값.
    /// @return 하위 16 비트로 narrowing 된 uint16_t (현 사용 범위 bit 0~5 는 안전).
    constexpr uint16_t ToBits(PhysicsLayer l)
    {
        return static_cast<uint16_t>(static_cast<uint64_t>(l));
    }

    /// @brief Player 가 부딪힐 대상 레이어 조합 (Enemy + BulletEnemy + Wall + Pickup).
    constexpr PhysicsLayer PlayerMask = PhysicsLayer::Enemy | PhysicsLayer::BulletEnemy | PhysicsLayer::Wall | PhysicsLayer::Pickup;
    /// @brief Enemy 가 부딪힐 대상 레이어 조합 (Player + BulletPlayer + Wall).
    constexpr PhysicsLayer EnemyMask  = PhysicsLayer::Player | PhysicsLayer::BulletPlayer | PhysicsLayer::Wall;
    /// @brief Wall 이 부딪힐 대상 레이어 조합 (Player + Enemy + BulletPlayer + BulletEnemy).
    constexpr PhysicsLayer WallMask   = PhysicsLayer::Player | PhysicsLayer::Enemy | PhysicsLayer::BulletPlayer | PhysicsLayer::BulletEnemy;
}

#endif // __MYAPP_PHYSICS_FILTER_H__
