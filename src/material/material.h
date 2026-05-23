/**
 * @file material.h
 * @brief Unity `material.SetXxx` 정통 — 셰이더 schema 무관 properties bag.
 *
 * @details
 *  ### 핵심 철학 (EngineAPI §3.7 / §4.3 / §4.5)
 *  Material 은 **"이 instance 가 어떤 컨텐츠 / 어떤 종류의 렌더링인가"** 만 선언한다.
 *  셰이더 schema 는 `Program::UniformCache` 가, GL 송신은 `PropertyBlockSetter::Set` 가 담당 —
 *  *책임 3분할 (값 / 스키마 / 송신)* 로 OCP 확보.
 *
 *  ### 구성 (3 reference + 1 enum + 6 typed map)
 *  - **Properties bag**: name 키 기반 typed-map (Float/Int/Vec3/Vec4/Mat4/Texture) —
 *    `SJH::Uniforms::Set*(Material&, ...)` 가 *store 만*. 셰이더가 안 받는 properties 는 silent skip.
 *  - **Program 참조** (비소유) + **UniformCache 참조** — `SetProgram(p)` 가 Observer 등록 + cache 단축 lookup.
 *
 *  ### Observer cascade — dangling 구조적 차단
 *  	Properties bag 은 *유지* — 재바인딩 가능 (Unity 정통).
 *  1. `Program::~Program`
 *  2. 등록된 모든 Material 의 `OnProgramReleased(this)
 *  3. `mProgram = nullptr` + cache clear.
 *
 *  ### 2-layer uniform 송신 (§4.5) — Material 의 책임 경계
 *  | 데이터 | 담당 | 비고 |
 *  |---|---|---|
 *  | 사용자 컨텐츠 (color/texture/shininess/...) | **Material 한정** | properties bag 에 store |
 *  | uModel / uView / uProj / viewPos / 광원 | **씬 전역** | MeshPassProcessor / SceneRenderer 직접 송신 |
 *
 *  *DrawCommand 마다 변하거나 모든 Material 공통* 인 uniform 은 Material 책임 아님 — SSoT 위반 회피.
 */
#ifndef __SJH_MATERIAL_H__
#define __SJH_MATERIAL_H__

#include "GL/gl3w.h"
#include "common/common.h"
#include "material/material_property_block.h" // Unity MaterialPropertyBlock 정통 — typed properties bag.
#include "material/pass.h"                    // Pass::Kind / DefaultPipelineStateOf — Material 의 렌더링 의도 선언.
#include "program/program.h"
#include <string>
#include <unordered_map>
#include <vmath.h>

namespace SJH
{
	class Texture; // 비소유 관찰자.

	CLASS_PTR(Material);

	/// @brief Unity Material 정통 — Pass.Kind + Program 참조 + MaterialPropertyBlock 보유.
	/// @details 책임 분할 (SP-Material-Split):
	///   - **본 클래스**: 메타 정보 (Program 참조 + Pass.Kind) + PropertyBlock 1 개 owner
	///   - **`MaterialPropertyBlock`**: 셰이더 무관 typed properties (Floats/Vec3s/Textures/...)
	///   - **`PropertyBlockSetter`**: block + Program → GL 송신 (Applier 패턴)
	class Material
	{
	  public:
		/// @brief 텍스처 바인딩 — `MaterialPropertyBlock::TextureBinding` forwarding alias (외부 호환).
		using TextureBinding = MaterialPropertyBlock::TextureBinding;

		// === Factory ==========================================================
		static MaterialUPtr Create()
		{
			return MaterialUPtr(new Material());
		}

		// === Lifetime — Observer cascade ====================================
		~Material()
		{
			if (mProgram)
				mProgram->UnregisterMaterial(this);
		}

		Material(const Material &other)
		{
			CopyFrom(other);
		}

		Material &operator=(const Material &other)
		{
			if (this != &other)
			{
				ReleaseProgram();
				CopyFrom(other);
			}
			return *this;
		}

		Material(Material &&) = delete;
		
		Material &operator=(Material &&) = delete;

		/// @brief Program 주입. 이전 Program 은 Unregister, 새 Program 에 Register + cache 참조.
		Material &SetProgram(const Program *program)
		{
			if (mProgram == program)
				abort();
			ReleaseProgram();
			mProgram = program;
			mCache = program ? &program->GetUniformCache() : nullptr;
			if (mProgram)
				mProgram->RegisterMaterial(this);
			return *this;
		}
		const Program *GetProgram() const
		{
			return mProgram;
		}
		const UniformCache *GetCache() const
		{
			return mCache;
		}

		/// @brief Program::~Program 의 cascade — mProgram + cache reference clear.
		/// @details Properties bag 은 *유지* — 다른 Program 으로 재바인딩 가능 (Unity 정통).
		void OnProgramReleased(const Program *releasing)
		{
			if (mProgram == releasing)
			{
				mProgram = nullptr;
				mCache = nullptr;
			}
		}

		/// @brief Unity MaterialPropertyBlock 정통 — 6 typed maps 통합 보유.
		/// @details 외부 접근: `mat.Properties.Floats["..."]` / `mat.Properties.Textures["..."]`.
		///          Setter family (`Uniforms::Set*(Material&, ...)`) 가 내부적으로 이 block 에 store.
		MaterialPropertyBlock Properties;

		/// @brief Pass 종류 변경 — (SetProgram 처럼 chain 가능).
		/// @details MeshPassProcessor 가 이 값 보고 queue/blend/depth/cull 자동 적용.
		///   기본 Opaque. Transparent / AlphaTest / Skybox 시 한 줄 호출:
		///   `mat->SetPass(Pass::Kind::Transparent)` — depth write off + blend on + queue 3000 자동.
		Material &SetPass(Pass::Kind k)
		{
			mPassKind = k;
			return *this;
		}

		/// @brief 현재 Pass 종류.
		Pass::Kind GetPass() const
		{
			return mPassKind;
		}

		/// @brief 자동 도출 queue layer — `Pass::QueueOf(PassKind)` alias.
		/// @details Material 단위 *절대 queue override* 는 *외부 API 미노출*
		///   Filament/Unreal/Cocos 정통 — Material 측은 "어떤 종류" 만, queue 값은 *PassKind 의 파생*.
		///   per-instance 미세 순서 조정은 `MeshRenderer::QueueOffset` 으로.
		int GetQueueLayer() const
		{
			return Pass::QueueOf(mPassKind);
		}

		/// @brief 공유 템플릿 -> per-use 가변 인스턴스 복제. Observer 등록 갱신.
		MaterialUPtr Clone() const
		{
			return MaterialUPtr(new Material(*this));
		}

	  private:
		Material() = default;

		void CopyFrom(const Material &other)
		{
			Properties = other.Properties; // ★ MaterialPropertyBlock 통째로 복사 (6 typed map 자동)
			mPassKind = other.mPassKind;   // Pass 의도 — Clone 시 Transparent 유지.
			mProgram = other.mProgram;
			mCache = other.mCache;
			if (mProgram)
				mProgram->RegisterMaterial(this); // 새 인스턴스로 register
		}

		void ReleaseProgram()
		{
			if (mProgram)
				mProgram->UnregisterMaterial(this);
			mProgram = nullptr;
			mCache = nullptr;
		}

		Pass::Kind mPassKind = Pass::Kind::Opaque; ///< SetPass / GetPass / GetQueueLayer 가 모두 이 값 도출.
		const Program *mProgram = nullptr;         ///< 비소유. SetProgram/Release/cascade 가 lifecycle 관리.
		const UniformCache *mCache = nullptr;      ///< Program 의 cache 참조 — 셰이더 schema 단축 lookup.
	};
} // namespace SJH

#endif // __SJH_MATERIAL_H__
