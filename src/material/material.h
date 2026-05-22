/**
 * @file material.h
 * @brief Unity-식 Material — *셰이더 schema 무관 properties bag* + Program 참조 + UniformCache 참조.
 *
 * @details
 *  ### 책임 (SP6)
 *  - **Properties bag**: name 키 기반 typed-map (Float/Int/Vec3/Vec4/Mat4/Texture).
 *    셰이더가 `material.shininess` 같은 *임의 이름* 의 uniform 을 받든, postprocess 가 `uScene`
 *    sampler 를 받든, *Material 클래스 자체는 무관*. 셋업 시 `SetFloat / SetVec3 / SetTexture` 호출만.
 *  - **Program 참조** (비소유): `SetProgram(p)` 시 — RegisterMaterial (Observer 등록) +
 *    UniformCache reference 보유 (셰이더 schema 자기 단위 인식).
 *  - **Observer cascade**: Program 해제 시 `OnProgramReleased` cascade -> mProgram=nullptr +
 *    cache reference clear. dangling 구조적 차단.
 *
 *  ### Unity 매핑 (정통)
 *  - Unity `material.SetFloat("_Color", c)` ↔ `SJH::Uniforms::SetFloat(mat, "_Color", c)`.
 *  - 셋업과 송신 분리 — `SetXxx` 는 *bag 에 store*, `MaterialApplier::Apply` 가 *Apply 시점에
 *    UniformCache 교집합으로 일괄 GL 송신*. 셰이더가 안 받는 properties 는 silent skip.
 *
 *  ### 비-책임
 *  - ❌ 셰이더 schema 정의 — `Program::UniformCache` 가 cache build 시 자동 enumerate.
 *  - ❌ Texture / Program 데이터 *소유* — 모두 비소유 관찰자.
 *  - ❌ GL 호출 직접 — `MaterialApplier::Apply` 가 담당.
 *
 *  ### 모듈 위치
 *  - INTERFACE 라이브러리 (header-only). `program` 모듈에 *역방향 의존* — Program::RegisterMaterial 호출.
 *    `program -> material` 도 `Material::OnProgramReleased` cascade — 두 모듈 양방향 inline.
 */
#ifndef __SJH_MATERIAL_H__
#define __SJH_MATERIAL_H__

#include "common/common.h"
#include "material/pass.h"   // Pass::Kind / DefaultStateOf — Material 의 렌더링 의도 선언.
#include "program/program.h"
#include "GL/gl3w.h"
#include <string>
#include <unordered_map>
#include <vmath.h>

namespace SJH
{
    class Texture;   // 비소유 관찰자.

    CLASS_PTR(Material);

    /// @brief Unity Material 정통 — 셰이더 무관 properties bag + Program 참조 + UniformCache reference.
    class Material
    {
    public:
        /// @brief 텍스처 바인딩 — sampler unit 과 비소유 텍스처 관찰자.
        struct TextureBinding
        {
            const Texture* Tex  = nullptr;
            GLint          Unit = 0;
        };

        // === Factory ==========================================================
        static MaterialUPtr Create() { return MaterialUPtr(new Material()); }

        // === Lifetime — Observer cascade ====================================
        ~Material()
        {
            if (mProgram) mProgram->UnregisterMaterial(this);
        }

        Material(const Material& other) { CopyFrom(other); }
        Material& operator=(const Material& other)
        {
            if (this != &other) { ReleaseProgram(); CopyFrom(other); }
            return *this;
        }
        Material(Material&&)            = delete;
        Material& operator=(Material&&) = delete;

        // === Program — Observer 등록 + UniformCache reference ================
        /// @brief Program 주입. 이전 Program 은 Unregister, 새 Program 에 Register + cache 참조.
        Material& SetProgram(const Program* program)
        {
            if (mProgram == program) abort();
            ReleaseProgram();
            mProgram = program;
            mCache   = program ? &program->GetUniformCache() : nullptr;
            if (mProgram) mProgram->RegisterMaterial(this);
	    return *this;
        }
        const Program*      GetProgram() const { return mProgram; }
        const UniformCache* GetCache()   const { return mCache; }

        /// @brief Program::~Program 의 cascade — mProgram + cache reference clear.
        /// @details Properties bag 은 *유지* — 다른 Program 으로 재바인딩 가능 (Unity 정통).
        void OnProgramReleased(const Program* releasing)
        {
            if (mProgram == releasing)
            {
                mProgram = nullptr;
                mCache   = nullptr;
            }
        }

        // === Properties bag — variant typed maps =============================
        // 타입별 분리 map — type erasure 비용 회피. C++17 std::variant 보다 직접적.
        std::unordered_map<std::string, float>           Floats;
        std::unordered_map<std::string, int>             Ints;
        std::unordered_map<std::string, vmath::vec3>     Vec3s;
        std::unordered_map<std::string, vmath::vec4>     Vec4s;
        std::unordered_map<std::string, vmath::mat4>     Mat4s;
        std::unordered_map<std::string, TextureBinding>  Textures;

        // === Pass — 렌더링 의도 선언 (Filament/Unreal/Cocos 정통, 진실의 원천 단일화) ====
        /// @brief Pass 종류 변경 — fluent (SetProgram 처럼 chain 가능).
        /// @details RenderQueue 가 이 값 보고 queue/blend/depth/cull 자동 적용.
        ///   기본 Opaque. Transparent / AlphaTest / Skybox 시 한 줄 호출:
        ///   `mat->SetPass(Pass::Kind::Transparent)` — depth write off + blend on + queue 3000 자동.
        Material& SetPass(Pass::Kind k)
        {
            mPassKind = k;
            return *this;
        }

        /// @brief 현재 Pass 종류.
        Pass::Kind GetPass() const { return mPassKind; }

        /// @brief 자동 도출 queue layer — `Pass::QueueOf(PassKind)` alias.
        /// @details Material 단위 *절대 queue override* 는 *외부 API 미노출* — 진실의 원천 = PassKind 하나.
        ///   Filament/Unreal/Cocos 정통 — Material 측은 "어떤 종류" 만, queue 값은 *PassKind 의 파생*.
        ///   per-instance 미세 순서 조정은 `MeshRenderer::QueueOffset` 으로.
        int GetQueueLayer() const { return Pass::QueueOf(mPassKind); }

        // === Clone (Unity MID / Unreal MID 패턴) =============================
        /// @brief 공유 템플릿 -> per-use 가변 인스턴스 복제. Observer 등록 갱신.
        MaterialUPtr Clone() const { return MaterialUPtr(new Material(*this)); }

    private:
        Material() = default;

        void CopyFrom(const Material& other)
        {
            Floats   = other.Floats;
            Ints     = other.Ints;
            Vec3s    = other.Vec3s;
            Vec4s    = other.Vec4s;
            Mat4s    = other.Mat4s;
            Textures = other.Textures;
            mPassKind = other.mPassKind;   // Pass 의도 — Clone 시 Transparent 유지.
            mProgram  = other.mProgram;
            mCache    = other.mCache;
            if (mProgram) mProgram->RegisterMaterial(this);   // 새 인스턴스로 register
        }

        void ReleaseProgram()
        {
            if (mProgram) mProgram->UnregisterMaterial(this);
            mProgram = nullptr;
            mCache   = nullptr;
        }

        Pass::Kind          mPassKind = Pass::Kind::Opaque;  ///< 진실의 원천 — SetPass / GetPass / GetQueueLayer 가 모두 이 값 도출.
        const Program*      mProgram = nullptr;   ///< 비소유. SetProgram/Release/cascade 가 lifecycle 관리.
        const UniformCache* mCache   = nullptr;   ///< Program 의 cache 참조 — 셰이더 schema 단축 lookup.
    };
}

#endif // __SJH_MATERIAL_H__
