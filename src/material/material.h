/**
 * @file material.h
 * @brief Unity Material 정통 — *셰이더 의도 선언* + *컨텐츠 properties bag* + *Instance metadata*.
 *
 * @details
 *  ### 구성 4 요소 (책임 분할)
 *  - `mPassKind` — *어떤 종류의 렌더링* (Pass::Kind, GL state 자동 도출 — SSoT)
 *  - `mProgram` (비소유) — 셰이더 schema 출처 (Observer 등록)
 *  - `Properties` (`MaterialPropertyBlock`) — *셰이더 무관 typed properties*
 *  - `IsInstance` + `OriginalMaterial` — Clone 추적 (Unreal `UMaterialInstanceDynamic::Parent` 정통)
 *
 *  ### 정통 매핑
 *  | 우리 | Unity | Unreal |
 *  |---|---|---|
 *  | `CreateSharedMaterial` | `sharedMaterial` getter | `UMaterialInterface` |
 *  | `CreateMaterialInstanceFrom` + auto Clone | `material` getter (자동 Clone) | `CreateDynamicMaterialInstance` |
 *  | `IsInstance` / `OriginalMaterial` | Inspector "(Instance)" 표시 | `Parent` 멤버 |
 *
 *  ### SSoT 강제 — *Material 인스턴스 생성의 유일한 진입점*
 *  - `Material::Create()` — 공유 원본 (ResourceRegistry 가 owner)
 *  - `ResourceRegistry::CreateMaterialInstanceFrom(key, template)` — *유일한 Clone 호출자*
 *  - `Clone()` 은 **private + friend ResourceRegistry** — 외부 직접 호출 컴파일 차단 (책임 분산 방지)
 *
 *  자세한 흐름: `EngineAPI.md` §3.7 / §4.3, `architecture.md` §11.3.
 */
#ifndef __SJH_MATERIAL_H__
#define __SJH_MATERIAL_H__

#include "GL/gl3w.h"
#include "common/common.h"
#include "material/material_property_block.h"
#include "material/pass.h"
#include "program/program.h"
#include <string>
#include <unordered_map>
#include <vmath.h>

namespace SJH
{
	class Texture;
	class ResourceRegistry; // friend — Clone() 의 유일한 호출자.

	CLASS_PTR(Material);

	/// @brief Unity Material 정통 — Pass.Kind (SSoT) + Program 참조 + PropertyBlock + Instance metadata.
	class Material
	{
	  public:
		/// @brief 텍스처 바인딩 — `MaterialPropertyBlock::TextureBinding` forwarding alias.
		using TextureBinding = MaterialPropertyBlock::TextureBinding;

		// ── Factory ───────────────────────────────────────────
		static MaterialUPtr Create()
		{
			return MaterialUPtr(new Material());
		}

		// ── Lifetime (Observer cascade — Program::~Program 가 OnProgramReleased 호출) ──
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

		// ── Program 참조 (Observer 등록) ───────────────────────
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
		const Program *GetProgram() const { return mProgram; }
		const UniformCache *GetCache() const { return mCache; }

		/// @brief Program::~Program cascade 진입점 — Properties bag 은 *유지* (다른 Program 재바인딩 가능).
		void OnProgramReleased(const Program *releasing)
		{
			if (mProgram == releasing)
			{
				mProgram = nullptr;
				mCache = nullptr;
			}
		}

		// ── Properties bag (Unity MaterialPropertyBlock 정통) ─
		/// @brief 외부 접근: `mat.Properties.Floats["..."]`. Setter family (`Uniforms::Set*(Material&, ...)`) 가 store.
		MaterialPropertyBlock Properties;

		// ── Pass.Kind (GL state SSoT — Cocos technique 정통) ──
		/// @brief Pass 종류 변경 — fluent setter. `SetPass(Transparent)` 한 줄로 depth/blend/queue 자동.
		Material &SetPass(Pass::Kind k)
		{
			mPassKind = k;
			return *this;
		}

		Pass::Kind GetPass() const { return mPassKind; }

		/// @brief 자동 도출 queue layer (`Pass::QueueOf(PassKind)` alias).
		///   Filament/Unreal/Cocos 정통 — Material 은 "어떤 종류" 만, per-instance 미세 순서는 `MeshRenderer::QueueOffset`.
		int GetQueueLayer() const { return Pass::QueueOf(mPassKind); }

		// ── Instance metadata (Unreal `UMaterialInstanceDynamic::Parent` 정통, 읽기 전용) ──
		/// @brief Clone 결과 인스턴스 여부. `Create()` 결과 = false, `Clone()` 결과 = true.
		bool IsInstance = false;

		/// @brief Clone 의 *직접 원본* — root 는 `GetRootOriginal()` 가 캐시.
		/// @details
		///   - `mutable` — 본인 슬롯은 `GetRootOriginal()` 경로 압축 캐시 쓰기를 위해
		mutable const Material *OriginalMaterial = nullptr;

		/// @brief Chain of Clones 의 *최상위 root* — direct parent 따라 거슬러 올라감.
		///   첫 호출에 root 를 OriginalMaterial 슬롯에 *경로 압축* — 다음 호출은 O(1).
		const Material *GetRootOriginal() const
		{
			const Material *p = this;
			while (p->OriginalMaterial)
				p = p->OriginalMaterial;
			return this->OriginalMaterial = p; // 경로 압축 — mutable 슬롯에 cache.
		}

	  private:
		Material() = default;

		/// @brief 공유 템플릿 -> per-use 가변 인스턴스 복제. *private* — SSoT 강제.
		/// @details `ResourceRegistry::CreateMaterialInstanceFrom` 만 호출 (friend).
		MaterialUPtr Clone() const
		{
			auto clone = MaterialUPtr(new Material(*this));
			clone->IsInstance = true;
			clone->OriginalMaterial = this;
			return clone;
		}
		friend class ResourceRegistry;

		void CopyFrom(const Material &other)
		{
			Properties = other.Properties; // MaterialPropertyBlock 통째로 복사 (6 typed map 자동)
			mPassKind = other.mPassKind;   // Pass 의도 — Clone 시 Transparent 유지.
			mProgram = other.mProgram;
			mCache = other.mCache;
			if (mProgram)
				mProgram->RegisterMaterial(this);
		}

		void ReleaseProgram()
		{
			if (mProgram)
				mProgram->UnregisterMaterial(this);
			mProgram = nullptr;
			mCache = nullptr;
		}

		Pass::Kind mPassKind = Pass::Kind::Opaque;
		const Program *mProgram = nullptr;
		const UniformCache *mCache = nullptr;
	};
} // namespace SJH

#endif // __SJH_MATERIAL_H__
