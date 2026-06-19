/**
 * @file sprite_resources.h
 * @brief Sprite 공유 자원(plane Mesh + per-instance Material) ResourceRegistry 해결 자유 함수.
 *
 * @details
 *  ### 거주지 (2026-06-11 E6 사이클 해소 - D8 DI 전환)
 *  과거 @c SpriteRenderer 생성자가 ResourceRegistry 를 자가 호출해 plane/program/material 을
 *  lazy 해결하던 로직(= sprite -> resource_registry 역의존)을 본 모듈(rr)로 이주했다.
 *  @c SpriteRenderer 는 이제 (Mesh*, Material*) 를 *주입*받고, 호출자가 본 함수로 자원을 해결한다.
 *  rr 은 sprite/object/material/program 에 단방향 의존하므로 사이클이 생기지 않는다.
 *
 *  ### ResourceRegistry 키 컨벤션 ('_' 접두 - 사용자 namespace 격리)
 *  - @c "_sprite_plane"             (Mesh)
 *  - @c "_sprite_billboard_program" (Program)
 *  - @c "_sprite_billboard"         (template SharedMaterial)
 *  - @c "_sprite_inst_N"            (per-instance MaterialInstance, N = 단조 증가 counter)
 */

#ifndef __SJH_RESOURCE_REGISTRY_SPRITE_RESOURCES_H__
#define __SJH_RESOURCE_REGISTRY_SPRITE_RESOURCES_H__

namespace SJH
{
	class Mesh;
	class Material;
}
namespace SJH::Sprite
{
	class UniformAtlas;
}

namespace SJH::SpriteResources
{
	/// @brief @c "_sprite_plane" (sprite 전용 billboard plane Mesh) 최초 1회 생성/등록 후 반환.
	/// @return 공유 plane Mesh (비소유 - ResourceRegistry 세션수명 소유). 실패 시 nullptr.
	SJH::Mesh *EnsureSharedPlane();

	/// @brief @p atlas 용 per-instance MaterialInstance (@c "_sprite_inst_N") 생성 후 반환.
	/// @details template SharedMaterial(@c "_sprite_billboard") + billboard_atlas program 을 최초 1회
	///          해결한 뒤 인스턴스를 복제하고 uAtlas/uUvRect/uFlipX/uTint 초기값을 세팅한다.
	/// @param atlas 사용할 @c UniformAtlas. @c nullptr 이면 nullptr 반환 (빈 SpriteRenderer).
	/// @return per-instance Material (비소유 - ResourceRegistry 세션수명 소유). 실패/atlas null 시 nullptr.
	SJH::Material *CreateInstanceMaterial(SJH::Sprite::UniformAtlas *atlas);
} // namespace SJH::SpriteResources

#endif // __SJH_RESOURCE_REGISTRY_SPRITE_RESOURCES_H__
