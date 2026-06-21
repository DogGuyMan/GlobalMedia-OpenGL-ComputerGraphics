/**
 * @file material.h
 * @brief Unity Material 정통 - *셰이더 의도 선언* + *컨텐츠 properties bag* + *Instance metadata*.
 *
 * @details
 *  ### 책임
 *  - `mPassKind` - *어떤 종류의 렌더링* (Pass::Kind, GL state 자동 도출 - SSoT)
 *  - `mProgram` (비소유) - 셰이더 schema 출처 + `EagerBuild` 의 검증 기준
 *  - `Properties` (`MaterialPropertyBlock`) - *셰이더 무관 typed properties*
 *  - `IsInstance` + `OriginalMaterial` - Clone 추적 (Unreal `UMaterialInstanceDynamic::Parent` 정통)
 *
 *  ### 비-책임
 *  - [X] GL 호출 - uniform 송신은 draw 시점 `MeshPassProcessor` (값=UBO 멤버, sampler=BindSamplers) 책임.
 *  - [X] 텍스처 GPU 업로드 - `Texture` / `ResourceRegistry` 책임.
 *
 *  ### 정통 매핑
 *  | 우리 | Unity | Unreal |
 *  |---|---|---|
 *  | `CreateSharedMaterial` | `sharedMaterial` getter | `UMaterialInterface` |
 *  | `CreateMaterialInstanceFrom` + auto Clone | `material` getter (자동 Clone) | `CreateDynamicMaterialInstance` |
 *  | `IsInstance` / `OriginalMaterial` | Inspector "(Instance)" 표시 | `Parent` 멤버 |
 *
 *  ### SSoT 강제 - *Material 인스턴스 생성의 유일한 진입점*
 *  - `Material::Create()` - 공유 원본 (ResourceRegistry 가 owner)
 *  - `ResourceRegistry::CreateMaterialInstanceFrom(key, template)` - *유일한 Clone 호출자*
 *  - `Clone()` 은 **private + friend ResourceRegistry** - 외부 직접 호출 컴파일 차단 (책임 분산 방지)
 *
 *  ### Lifetime - Program 보다 *먼저* 죽음 보장 (컨벤션)
 *  Material 은 ResourceRegistry 가 보유. Program 도 ResourceRegistry 위탁 시 (future
 *  `SP-ProgramRegistry`) destroy 순서가 Material -> Program 자동 보장 -> `mProgram` raw
 *  pointer dangling 불가. 현 시점 `tweeny_demo` 의 명시 `mProgram.reset()` 은 *컨벤션 위반*
 *  으로 같은 SP 에서 제거 예정.
 *
 *  ### EagerBuild - Properties <-> Program schema 동기화 (SetProgram 시점)
 *  `SetProgram(prog)` 호출 시 `Properties` 의 7 typed map 을 순회하며
 *  `prog->GetLocation(name) < 0` (active uniform 아님) 인 key 는 **prune** 한다.
 *  런타임상 draw 시점 UBO 멤버 업로드/BindSamplers 가 셰이더에 없는 key 를 자연 skip 해
 *  무해하지만, *오타 조기 발견* + *진단 가시성* 차원에서 사전 정리. stderr 출력.
 *
 *  자세한 흐름: `EngineAPI.md` sec.3.7 / sec.4.3, `architecture.md` sec.11.3.
 *
 * @note `Clone()` 은 `ResourceRegistry` friend 전용 - 외부에서 직접 호출 시 컴파일 에러.
 */
#ifndef __SJH_MATERIAL_H__
#define __SJH_MATERIAL_H__

#include "GL/gl3w.h"
#include "common/common.h"
#include "material/material_property_block.h"
#include "material/pass.h"
#include "program/program.h"
#include <cstdio>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace SJH
{
	class Texture;
	class ResourceRegistry; // friend - Clone() 의 유일한 호출자.

	CLASS_PTR(Material);

	/**
	 * @brief Unity Material 정통 - Pass.Kind (SSoT) + Program 참조 + PropertyBlock + Instance metadata.
	 * @details
	 *  - 자원 소유권은 `ResourceRegistry` 보유 (`unique_ptr`).
	 *  - `SetPass(Kind)` 한 줄로 depth / blend / queue 7 GL state 자동 결정 (SSoT).
	 *  - `Properties` bag 에 typed 값을 적재 -> draw 시점 `MeshPassProcessor` 가 UBO 멤버 + sampler 로 송신.
	 *  - `Clone()` 은 private + `ResourceRegistry` friend 전용 - 외부 직접 호출 불가.
	 */
	class Material
	{
	  public:
		/// @brief 텍스처 바인딩 - `MaterialPropertyBlock::TextureBinding` forwarding alias.
		using TextureBinding = MaterialPropertyBlock::TextureBinding;

		// -- Factory -------------------------------------------
		/// @brief 빈 Material 생성 - 공유 원본용. ResourceRegistry::CreateSharedMaterial 이 wrap.
		/// @details `new Material()` 직접 호출 대신 이 factory 를 사용해야 CLASS_PTR 스마트포인터 패턴과 일관.
		static MaterialUPtr Create()
		{
			return MaterialUPtr(new Material());
		}

		~Material()
		{
		}

		Material &operator=(const Material &other) = delete;
		Material(Material &&) = delete;
		Material &operator=(Material &&) = delete;

		// -- Program 참조 (Unity Material.shader 정통) ----------
		/// @brief Program 주입 + EagerBuild - Properties 가 Program schema 와 사전 동기화.
		/// @details Lifetime 컨벤션: Program 이 Material 보다 *더 오래* 살아야 함
		///          (ResourceRegistry 가 둘 다 보유 시 destroy 순서로 자동 보장).
		/// @param program 비소유 포인터 - 호출자 또는 ResourceRegistry 가 lifetime 관리.
		/// @return `*this` - fluent setter.
		Material &SetProgram(const Program *program)
		{
			mProgram = program;
			return *this;
		}

		/// @brief 현재 연결된 Program (비소유 관찰자 포인터). 미설정 시 nullptr.
		const Program *GetProgram() const
		{
			return mProgram;
		}

		// -- Properties bag (Unity MaterialPropertyBlock 정통) -
		/// @brief 셰이더 uniform 값 집합. `Uniforms::Set*(mat, name, value)` family 로 값을 적재.
		/// @details `mat.Properties.Floats["uShininess"]` 와 같이 직접 접근도 가능하나,
		///          `Uniforms::SetFloat(mat, "uShininess", 32.f)` API 경유가 관용.
		MaterialPropertyBlock Properties;

		// -- Pass.Kind (GL state SSoT - Cocos technique 정통) --
		/// @brief Pass 종류 변경 - fluent setter. `SetPass(Transparent)` 한 줄로 depth/blend/queue 자동.
		/// @param k 렌더링 의도 (Opaque / AlphaTest / Transparent / Outline 등).
		/// @return `*this` - fluent setter.
		Material &SetPass(Pass::Kind k)
		{
			mPassKind = k;
			return *this;
		}

		/// @brief 현재 Pass 종류 반환.
		Pass::Kind GetPass() const
		{
			return mPassKind;
		}

		/// @brief 자동 도출 queue layer (`Pass::QueueOf(PassKind)` alias).
		/// @details MeshPassProcessor 의 sort key 로 사용 - 값이 작을수록 먼저 draw.
		int GetQueueLayer() const
		{
			return Pass::QueueOf(mPassKind);
		}

		// -- Instance metadata (Unreal `UMaterialInstanceDynamic::Parent` 정통, 읽기 전용) --
		/// @brief Clone 결과 인스턴스 여부. `Create()` 결과 = false, `Clone()` 결과 = true.
		bool IsInstance = false;

		/// @brief Clone 의 *직접 원본* - root 는 `GetRootOriginal()` 가 캐시.
		/// @details
		///   - `mutable` - 본인 슬롯은 `GetRootOriginal()` 경로 압축 캐시 쓰기를 위해
		mutable const Material *OriginalMaterial = nullptr;

		/// @brief Chain of Clones 의 *최상위 root* - direct parent 따라 거슬러 올라감.
		/// @details Clone 체인을 root 까지 거슬러 올라 OriginalMaterial 슬롯에 *경로 압축* (mutable 캐시).
		/// @return 공유 원본 Material 포인터. @warning 현재 호출자 없음 - 진짜 root(@c OriginalMaterial==nullptr)에서 호출하면 자기참조를 세팅해 *재호출 시 무한루프* 가 되는 잠재 버그 (코드 가드 필요).
		const Material *GetRootOriginal() const
		{
			const Material *p = this;
			while (p->OriginalMaterial)
				p = p->OriginalMaterial;
			return this->OriginalMaterial = p; // 경로 압축 - mutable 슬롯에 cache.
		}

	  private:
		friend class ResourceRegistry;
		Material(const Material &other)
		{
			CopyFrom(other);
		}

		Material() = default;

		/// @brief 공유 템플릿 -> per-use 가변 인스턴스 복제. *private* - SSoT 강제.
		/// @details `ResourceRegistry::CreateMaterialInstanceFrom` 만 호출 (friend).
		MaterialUPtr Clone() const
		{
			auto clone = MaterialUPtr(new Material(*this));
			clone->IsInstance = true;
			clone->OriginalMaterial = this;
			return clone;
		}

		/// @brief 복사 생성 헬퍼 - Clone 내부에서만 사용.
		/// @details Properties(typed map 7종) + PassKind + Program 참조를 일괄 복사.
		void CopyFrom(const Material &other)
		{
			Properties = other.Properties; // MaterialPropertyBlock 통째로 복사 (7 typed map 자동)
			mPassKind = other.mPassKind;   // Pass 의도 - Clone 시 Transparent 유지.
			mProgram = other.mProgram;     // Program 참조 승계 (raw pointer - Program 이 더 오래 사는 컨벤션).
		}

		Pass::Kind mPassKind = Pass::Kind::Opaque; ///< 렌더링 의도 종류 - GL state SSoT.
		const Program *mProgram = nullptr; ///< 비소유 - owner 는 ResourceRegistry (or 데모 임시).
	};
} // namespace SJH

#endif // __SJH_MATERIAL_H__
