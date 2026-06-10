/**
 * @file SpriteLayerFactory.h
 * @brief "텍스처 1장 -> atlas find-or-create + SpriteRenderer(+옵션 애니) 부착" 공유 헬퍼.
 *
 * @details
 *  ### 책임
 *  - @c EntityTextureConfig 설정 한 건에서 @c UniformAtlas find-or-create + @c SpriteRenderer 부착.
 *  - ColCount > 1 이면 @c SpriteSequencePlayable(loop) 자동 부착 + 즉시 재생.
 *  - 3곳의 빌더(PlayerBuilder 8그룹 / PlayerActor InitSprite / EnemyBuilder) 의 동일 패턴 dedup.
 *
 *  ### 비-책임
 *  - [X] child/owner 결정 (어느 액터에 부착할지) - caller 가 @p target 으로 결정.
 *  - [X] SpriteRenderer 후속 설정 (flip 이외 속성) - caller 가 반환된 포인터로 직접 설정.
 *  - [X] 애니 프레임 클립 범위 설정 (0~ColCount 고정) - 상세 클립은 caller 가 별도 생성.
 *
 *  ### 정통 매핑
 *  - Cocos2D @c Sprite::create(filename) : atlas 조회 + 스프라이트 노드 반환.
 *
 * @note atlas 로드/검증 실패 시 @c spdlog::error 로 기록 후 nullptr 반환.
 *       caller 는 반환값 nullptr 체크 필수.
 */
#ifndef _TOPDOWNSHOOTER_PLAYABLE_SPRITE_LAYER_FACTORY__
#define _TOPDOWNSHOOTER_PLAYABLE_SPRITE_LAYER_FACTORY__

#include "Playable/Constants.h" // EntityTextureConfig (값 파라미터 - 완전형 필요)

// fwd-decl - 헤더 표면엔 완전형 불필요 (.cpp 에서 해소). 소비자(Entity/Bootstrap)가
// 무거운 sprite/registry 헤더를 transitively 끌어오지 않도록 의존 표면을 최소화.
namespace SJH
{
	class ResourceRegistry;
	namespace Scene
	{
		class Actor;
	}
	namespace Sprite
	{
		class SpriteRenderer;
	}
} // namespace SJH

namespace TopdownShooter::Playable
{
	/// @brief "텍스처 1장 -> atlas find-or-create + SpriteRenderer(+옵션 애니) 부착" 공유 헬퍼.
	/// @details 3-빌더의 동일 패턴 dedup - PlayerBuilder 8그룹 / PlayerActor InitSprite / EnemyBuilder.
	///          도메인 의존 0 (reg/texCfg 만 받는 presentation 헬퍼). child/owner-direct 결정은 caller 몫.
	/// @param target  SpriteRenderer 를 부착할 액터.
	/// @param reg     아틀라스 캐시 (path 키 find-or-create).
	/// @param t       텍스처 1장 config (path/grid/flip/drawOrder).
	/// @param fps     ColCount>1 일 때 애니 초당 프레임.
	/// @return 부착된 SpriteRenderer* (atlas 로드/검증 실패 시 nullptr - 에러 로그).
	SJH::Sprite::SpriteRenderer *AttachSpriteLayer(
	    SJH::Scene::Actor &target, SJH::ResourceRegistry &reg,
	    const EntityTextureConfig &t, float fps);
} // namespace TopdownShooter::Playable

#endif //_TOPDOWNSHOOTER_PLAYABLE_SPRITE_LAYER_FACTORY__
