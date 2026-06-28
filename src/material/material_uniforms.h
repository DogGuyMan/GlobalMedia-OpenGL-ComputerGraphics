/**
 * @file material_uniforms.h
 * @brief Material properties bag 의 typed store family - Unity `material.SetXxx` 정통 (EngineAPI sec.4.3 / sec.4.5).
 *
 * @details
 *  ### 책임
 *  - `Material` 의 typed map (Floats / Ints / Vec2s / Vec3s / Vec4s / Mat4s / Textures) 에 *값만 기록*.
 *  - `SetTexture` - Texture 비소유 포인터 + sampler unit 을 `TextureBinding` 으로 묶어 Textures map 저장.
 *
 *  ### 비-책임
 *  - [X] GL `glUniform*` / UBO 업로드 - draw 시점 `MeshPassProcessor` 가 송신 (값=UBO 멤버, sampler=loose).
 *  - [X] Program schema 검증 - `Material::SetProgram` 의 EagerBuild 가 사전 prune.
 *
 *  ### 핵심 - *store-only* semantics
 *  본 family 는 GL 호출을 **하지 않는다**. Material 의 typed map (Floats/Ints/Vec3s/...) 에 *값만 기록*.
 *  실제 송신은 draw 시점 `MeshPassProcessor` 가: (a) 값 uniform -> `Program::UpdateUniformMember`
 *  (author 이름 매칭 UBO 멤버 업로드), (b) sampler -> `BindSamplers` (live `GetLocation` + glUniform1i).
 *  (Phase C - 구 `PropertyBlockSetter`/`UniformCache` 교집합 경로 폐지.)
 *
 *  ### 2-layer uniform 송신 (sec.4.5) - 본 family 의 자리
 *  | family | 인자 | 동작 | 용도 |
 *  |---|---|---|---|
 *  | **`Set*(Material&, ...)`** *(본 헤더)* | `Material&` | **store-only** | 사용자 컨텐츠 (color/texture/shininess) |
 *  | `Set*(const Program&, ...)` *(program_uniforms.h)* | `const Program&` | 즉시 GL 호출 | sampler unit / 잔여 loose 송신 |
 *
 *  ### OCP 함의
 *  셰이더 schema 가 바뀌어도 본 family 호출은 *그대로*. 셰이더에 *추가* 된 uniform 은
 *  setup 코드에서 `SetFloat / SetTexture` 한 줄 추가, *제거* 된 uniform 은 bag 에 남아도 silent skip.
 *  Material 클래스는 셰이더 schema 를 모른다 - 진실의 원천은 `Program` 의 UBO 멤버 맵 / `GetLocation`.
 *
 * @note 모든 함수는 `SJH::Uniforms` 네임스페이스 소속.
 *       관용적 사용: `namespace U = SJH::Uniforms; U::SetFloat(mat, "uShininess", 32.f);`
 */
#ifndef __SJH_MATERIAL_UNIFORMS_H__
#define __SJH_MATERIAL_UNIFORMS_H__

#include "GL/gl3w.h"
#include <glm/glm.hpp>

namespace SJH
{
	class Material;
	class Texture;

	namespace Uniforms
	{
		// === Material properties bag setter family ============================
		// 모두 bag 에 store 만 - 셰이더 무관. draw 시점 MeshPassProcessor 가 UBO/sampler 로 송신.

		/// @brief float uniform 값을 @p mat 의 Properties.Floats 에 store.
		/// @param mat  대상 Material (비const 참조).
		/// @param name 셰이더 uniform 이름 (예: @c "uShininess").
		/// @param v    저장할 값.
		void SetFloat(Material &mat, const char *name, float v);

		/// @brief int / bool uniform 값을 Properties.Ints 에 store.UploadMaterialUboMembers
		/// @details GL_BOOL uniform 도 이 함수로 저장 - UBO MaterialBlock 의 int 멤버로 업로드됨 ().
		/// @param mat  대상 Material.
		/// @param name 셰이더 uniform 이름.
		/// @param v    저장할 값.
		void SetInt(Material &mat, const char *name, int v);

		/// @brief vec2 uniform 값을 Properties.Vec2s 에 store.
		/// @param mat  대상 Material.
		/// @param name 셰이더 uniform 이름.
		/// @param v    저장할 값.
		void SetVec2(Material &mat, const char *name, const glm::vec2 &v);

		/// @brief vec3 uniform 값을 Properties.Vec3s 에 store.
		/// @param mat  대상 Material.
		/// @param name 셰이더 uniform 이름.
		/// @param v    저장할 값.
		void SetVec3(Material &mat, const char *name, const glm::vec3 &v);

		/// @brief vec4 uniform 값을 Properties.Vec4s 에 store.
		/// @param mat  대상 Material.
		/// @param name 셰이더 uniform 이름.
		/// @param v    저장할 값.
		void SetVec4(Material &mat, const char *name, const glm::vec4 &v);

		/// @brief mat4 uniform 값을 Properties.Mat4s 에 store.
		/// @param mat  대상 Material.
		/// @param name 셰이더 uniform 이름.
		/// @param v    저장할 값.
		void SetMat4(Material &mat, const char *name, const glm::mat4 &v);

		/// @brief Texture binding - Texture* 비소유 관찰자 + sampler unit 을 Properties.Textures 에 store.
		/// @details draw 시점 `MeshPassProcessor::BindSamplers` 가 (a) `glUniform1i(loc, unit)` 으로 sampler slot 송신 +
		///          (b) `DeviceContext::BindTexture(unit, texID)` 로 텍스처 바인딩.
		/// @param mat  대상 Material.
		/// @param name 셰이더 sampler uniform 이름 (예: @c "uAlbedo").
		/// @param tex  비소유 텍스처 포인터 - owner 는 ResourceRegistry.
		/// @param unit GL 텍스처 unit 번호 (0-based; @c GL_TEXTURE0 + unit).
		void SetTexture(Material &mat, const char *name, const Texture *tex, GLint unit);
	} // namespace Uniforms
} // namespace SJH

#endif // __SJH_MATERIAL_UNIFORMS_H__
