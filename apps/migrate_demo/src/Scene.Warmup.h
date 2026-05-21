#pragma once

/**
 * @file Scene.Warmup.h
 * @brief migrate_demo 씬 워밍업 — 레퍼런스 OpenGL-With-CMake/src/context/context.cpp 의
 *        Scene Graph + Materials + Lights 셋업을 SJH Actor/Component 그래프로 재현.
 *
 * @details
 *   ### 씬 구성 (context.cpp Init() 회귀)
 *   - Plane (marble diffuse + gray specular + shininess 128) — translate(0,-0.5,0), scale(10,1,10)
 *   - Box1  (container + dark_gray + shininess 16) — translate(-1,0.75,-4), rot(0,30,0), scale(1.5)
 *   - Box2  (container2 + container2_specular + shininess 64) — translate(0,0.75,2), rot(0,20,0), scale(1.5)
 *   - Outline (simple yellow shell) — Box2 자식, scale(1.05). queueLayer 1995 로 Box2(2000) 전에 그려짐.
 *   - Windows × 3 (alpha discard 텍스쳐) — translate(0/0.2/0.4 .x, 0.5, 4/5/6), queueLayer 3000
 *   - PointLight0 / PointLight1 / SpotLight 마커 큐브 (simple program, scale 0.1)
 *   - DirLight (마커 없음 — 방향만)
 *
 *   ### Compound Actor 컨벤션
 *   - Camera/DirLight/PointLight/SpotLight 는 free factory (compound_actor.h).
 *   - 광원 Actor 에 MeshRenderer 를 추가로 부착 — Light Component 와 type_index 충돌 없음.
 *
 *   ### 모자란 부분 (의도된 단순화)
 *   - **Stencil outline** — 레퍼런스는 stencil 마스킹으로 outline. SJH RenderSystem 은
 *     per-actor GL 상태 변경 미지원이므로 *shell scale 트릭* 으로 근사 (queueLayer ordering).
 *     완벽한 outline 은 RenderSystem 확장 필요 (TODO).
 *   - **FlashLight 모드 / ImGui 컨트롤** — ImGui 모듈 폐기 상태라 키 조작/슬라이더 생략.
 *   - **Depth func combo** — runtime 변경 UI 없음. GL_LESS (기본) 고정.
 */

#include "material/material_uniforms.h"
#include "object/light.h"
#include "object/mesh.h"
#include "program/program.h"
#include "resource_registry/image.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/components.h"
#include "scene/compound_actor.h"
#include "scene/scene.h"
#include <cstdint>
#include <memory>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>
#include <vmath.h>

namespace MigrateDemo::Scene
{
	// === Layer 정의 (postfx_demo 회귀) ===
	constexpr uint32_t LAYER_SCENE  = 1u; // 비트 0 — Plane/Box/Outline/Windows/Light 마커
	constexpr uint32_t LAYER_POSTFX = 2u; // 비트 1 — Screen quad (PostFXCamera 만 본다)

	// === Queue Layer 컨벤션 (Unity 정통) ===
	constexpr int QUEUE_OUTLINE     = 1995; // Opaque 직전 — 셸이 먼저, Box2 가 덮어서 rim 만 보임
	constexpr int QUEUE_OPAQUE      = 2000; // 기본 Opaque
	constexpr int QUEUE_TRANSPARENT = 3000; // Alpha discard 윈도우

	struct Programs
	{
		const SJH::Program *phong  = nullptr; // 씬 본체 (Plane / Box1 / Box2) — sampler2D material
		const SJH::Program *simple = nullptr; // 마커 큐브 / 아웃라인 — baseColor 단색
		const SJH::Program *window = nullptr; // 윈도우 — alpha discard
	};

	/// @brief 이미지 → 텍스쳐 워밍업. ResourceRegistry 에 일괄 등록.
	inline void WarmupAssets(SJH::ResourceRegistry &reg)
	{
		// === 절차적 이미지 — Phong specular 채널용 회색 ===
		auto imgDarkGray = SJH::Image::Create("img_dark_gray", 4, 4, 4);
		imgDarkGray->SetSingleColorImage(vmath::vec4(0.2f, 0.2f, 0.2f, 1.0f));
		auto imgGray = SJH::Image::Create("img_gray", 4, 4, 4);
		imgGray->SetSingleColorImage(vmath::vec4(0.4f, 0.4f, 0.4f, 1.0f));

		// === 파일 이미지 (resources/texture/*) — POST_BUILD 로 실행파일 옆에 복사됨 ===
		auto imgMarble        = SJH::Image::Load("img_marble",       "resources/texture/marble.jpg");
		auto imgContainer     = SJH::Image::Load("img_container",    "resources/texture/container.jpg");
		auto imgContainer2    = SJH::Image::Load("img_container2",   "resources/texture/container2.png");
		auto imgContainer2Spec = SJH::Image::Load("img_container2_spec",
		                                          "resources/texture/container2_specular.png");
		auto imgWindow        = SJH::Image::Load("img_window",
		                                         "resources/texture/blending_transparent_window.png");

		reg.CreateTexture("tex_dark_gray",        imgDarkGray.get());
		reg.CreateTexture("tex_gray",             imgGray.get());
		reg.CreateTexture("tex_marble",           imgMarble.get());
		reg.CreateTexture("tex_container",        imgContainer.get());
		reg.CreateTexture("tex_container2",       imgContainer2.get());
		reg.CreateTexture("tex_container2_spec",  imgContainer2Spec.get());
		reg.CreateTexture("tex_window",           imgWindow.get());

		// === Mesh — Box + Plane + ScreenQuad (postfx) ===
		reg.RegisterMesh("mesh_box",         SJH::Mesh::CreateBox());
		reg.RegisterMesh("mesh_plane",       SJH::Mesh::CreatePlane());
		reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());
	}

	namespace detail
	{
		/// @brief Phong material 헬퍼 — sampler2D material.diffuse / specular + shininess.
		inline SJH::Material *MakePhongMat(SJH::ResourceRegistry &reg,
		                                    const std::string &key,
		                                    const SJH::Program *prog,
		                                    const SJH::Texture *diffuse,
		                                    const SJH::Texture *specular,
		                                    float shininess)
		{
			auto *mat = reg.CreateMaterial(key);
			mat->SetProgram(prog);
			SJH::Uniforms::SetTexture(*mat, "material.diffuse",  diffuse,  0);
			SJH::Uniforms::SetTexture(*mat, "material.specular", specular, 1);
			SJH::Uniforms::SetFloat  (*mat, "material.shininess", shininess);
			return mat;
		}

		/// @brief Simple material — baseColor 단색.
		inline SJH::Material *MakeSimpleMat(SJH::ResourceRegistry &reg,
		                                     const std::string &key,
		                                     const SJH::Program *prog,
		                                     const vmath::vec4 &color)
		{
			auto *mat = reg.CreateMaterial(key);
			mat->SetProgram(prog);
			SJH::Uniforms::SetVec4(*mat, "baseColor", color);
			return mat;
		}

		/// @brief Window material — sampler tex0 (alpha discard).
		inline SJH::Material *MakeWindowMat(SJH::ResourceRegistry &reg,
		                                     const std::string &key,
		                                     const SJH::Program *prog,
		                                     const SJH::Texture *tex)
		{
			auto *mat = reg.CreateMaterial(key);
			mat->SetProgram(prog);
			SJH::Uniforms::SetTexture(*mat, "tex0", tex, 0);
			return mat;
		}
	} // namespace detail

	/// @brief 본체 액터 워밍업 — Plane / Box1 / Box2 (+ Outline) / Windows × 3.
	inline void WarmupActors(const Programs &progs, SJH::Scene::Director &dir)
	{
		auto &reg = SJH::ResourceRegistry::Get();

		auto *meshBox   = reg.FindMesh("mesh_box");
		auto *meshPlane = reg.FindMesh("mesh_plane");

		auto *texDarkGray       = reg.FindTexture("tex_dark_gray");
		auto *texGray           = reg.FindTexture("tex_gray");
		auto *texMarble         = reg.FindTexture("tex_marble");
		auto *texContainer      = reg.FindTexture("tex_container");
		auto *texContainer2     = reg.FindTexture("tex_container2");
		auto *texContainer2Spec = reg.FindTexture("tex_container2_spec");
		auto *texWindow         = reg.FindTexture("tex_window");

		// --- Plane (marble + gray + shininess 128) ---
		{
			auto plane = std::make_unique<SJH::Scene::Actor>("Plane");
			plane->SetLayer(LAYER_SCENE);
			plane->GetTransform().Translate = vmath::vec3(0.0f, -0.5f, 0.0f);
			plane->GetTransform().Scale     = vmath::vec3(10.0f, 1.0f, 10.0f);
			auto *mat = detail::MakePhongMat(reg, "mat_plane", progs.phong,
			                                   texMarble, texGray, 128.0f);
			plane->AddComponent<SJH::Scene::MeshRenderer>(meshPlane, mat, QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(plane));
		}

		// --- Box1 (container + dark_gray + shininess 16) ---
		{
			auto box = std::make_unique<SJH::Scene::Actor>("Box1");
			box->SetLayer(LAYER_SCENE);
			box->GetTransform().Translate = vmath::vec3(-1.0f, 0.75f, -4.0f);
			box->GetTransform().EulerRot  = vmath::vec3(0.0f, 30.0f, 0.0f);
			box->GetTransform().Scale     = vmath::vec3(1.5f, 1.5f, 1.5f);
			auto *mat = detail::MakePhongMat(reg, "mat_box1", progs.phong,
			                                   texContainer, texDarkGray, 16.0f);
			box->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat, QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(box));
		}

		// --- Box2 + Outline child ---
		{
			auto box = std::make_unique<SJH::Scene::Actor>("Box2");
			box->SetLayer(LAYER_SCENE);
			box->GetTransform().Translate = vmath::vec3(0.0f, 0.75f, 2.0f);
			box->GetTransform().EulerRot  = vmath::vec3(0.0f, 20.0f, 0.0f);
			box->GetTransform().Scale     = vmath::vec3(1.5f, 1.5f, 1.5f);
			auto *matBox2 = detail::MakePhongMat(reg, "mat_box2", progs.phong,
			                                       texContainer2, texContainer2Spec, 64.0f);
			box->AddComponent<SJH::Scene::MeshRenderer>(meshBox, matBox2, QUEUE_OPAQUE);

			// Outline 셸 — Box2 의 자식이므로 부모 transform 누적. scale 1.05 로 약간 큼.
			// queueLayer 1995 < 2000 이라 본체보다 먼저 그려져 rim 만 노출 (depth 트릭).
			auto outline = std::make_unique<SJH::Scene::Actor>("Outline");
			outline->SetLayer(LAYER_SCENE);
			outline->GetTransform().Scale = vmath::vec3(1.05f, 1.05f, 1.05f);
			auto *matOutline = detail::MakeSimpleMat(reg, "mat_outline", progs.simple,
			                                         vmath::vec4(1.0f, 1.0f, 0.5f, 1.0f));
			outline->AddComponent<SJH::Scene::MeshRenderer>(meshBox, matOutline, QUEUE_OUTLINE);
			box->AddChild(std::move(outline));

			dir.Root().AddChild(std::move(box));
		}

		// --- Windows × 3 (alpha discard) — y 같음, x 점진 +0.2 / z 4/5/6 ---
		{
			const vmath::vec3 positions[3] = {
			    vmath::vec3(0.0f, 0.5f, 4.0f),
			    vmath::vec3(0.2f, 0.5f, 5.0f),
			    vmath::vec3(0.4f, 0.5f, 6.0f),
			};
			for (int i = 0; i < 3; ++i)
			{
				auto win = std::make_unique<SJH::Scene::Actor>("Window" + std::to_string(i));
				win->SetLayer(LAYER_SCENE);
				win->GetTransform().Translate = positions[i];
				// 단일 머티리얼 공유 — Material::Clone 미사용 (셋업 동일).
				auto *mat = (i == 0)
				              ? detail::MakeWindowMat(reg, "mat_window", progs.window, texWindow)
				              : reg.FindMaterial("mat_window");
				win->AddComponent<SJH::Scene::MeshRenderer>(meshPlane, mat, QUEUE_TRANSPARENT);
				dir.Root().AddChild(std::move(win));
			}
		}
	}

	/// @brief 광원 워밍업 — DirLight + PointLight × 2 + SpotLight + 각 점/스포트 광원 마커 큐브.
	inline void WarmupLights(const Programs &progs, SJH::Scene::Director &dir)
	{
		auto &reg     = SJH::ResourceRegistry::Get();
		auto *meshBox = reg.FindMesh("mesh_box");

		// === DirLight (마커 없음 — 평행광이라 위치 무의미) ===
		// 레퍼런스 context.cpp: EulerRot {-90, 0, 0} → forward = (0,-1,0).
		{
			auto sun = SJH::Scene::CreateDirLightActor("Sun", vmath::vec3(0.0f, -1.0f, 0.0f));
			sun->SetLayer(LAYER_SCENE);
			auto *dl = sun->GetComponent<SJH::DirLight>();
			dl->Ambient  = vmath::vec3(1.0f, 1.0f, 1.0f);
			dl->Diffuse  = vmath::vec3(1.0f, 1.0f, 1.0f);
			dl->Specular = vmath::vec3(1.0f, 1.0f, 1.0f);
			dir.Root().AddChild(std::move(sun));
		}

		// === PointLight0 (orange) — translate(1.2, 1, 1), distance 50 ===
		{
			auto lamp = SJH::Scene::CreatePointLightActor("Lamp0",
			                                               vmath::vec3(1.2f, 1.0f, 1.0f), 50.0f);
			lamp->SetLayer(LAYER_SCENE);
			lamp->GetTransform().Scale = vmath::vec3(0.1f, 0.1f, 0.1f);

			auto *pl = lamp->GetComponent<SJH::PointLight>();
			pl->Ambient  = vmath::vec3(0.05f, 0.05f, 0.05f);
			pl->Diffuse  = vmath::vec3(0.8f, 0.4f, 0.2f);
			pl->Specular = vmath::vec3(1.0f, 1.0f, 1.0f);

			auto *mat = detail::MakeSimpleMat(reg, "mat_marker_lamp0", progs.simple,
			                                   vmath::vec4(pl->Diffuse, 1.0f));
			lamp->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat, QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(lamp));
		}

		// === PointLight1 (blue) — translate(-1.2, 1, -1), distance 50 ===
		{
			auto lamp = SJH::Scene::CreatePointLightActor("Lamp1",
			                                               vmath::vec3(-1.2f, 1.0f, -1.0f), 50.0f);
			lamp->SetLayer(LAYER_SCENE);
			lamp->GetTransform().Scale = vmath::vec3(0.1f, 0.1f, 0.1f);

			auto *pl = lamp->GetComponent<SJH::PointLight>();
			pl->Ambient  = vmath::vec3(0.05f, 0.05f, 0.05f);
			pl->Diffuse  = vmath::vec3(0.2f, 0.4f, 0.8f);
			pl->Specular = vmath::vec3(1.0f, 1.0f, 1.0f);

			auto *mat = detail::MakeSimpleMat(reg, "mat_marker_lamp1", progs.simple,
			                                   vmath::vec4(pl->Diffuse, 1.0f));
			lamp->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat, QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(lamp));
		}

		// === SpotLight — translate(1, 4, 4), 아래 방향, cutoff 5°/120° ===
		// 레퍼런스: CutoffAngleDeg=5, OuterCutoffAngleDeg=120, Distance=128.
		{
			auto spot = SJH::Scene::CreateSpotLightActor("Sun_Spot",
			                                              vmath::vec3(1.0f, 4.0f, 4.0f),
			                                              vmath::vec3(0.0f, -1.0f, 0.0f),
			                                              5.0f, 120.0f);
			spot->SetLayer(LAYER_SCENE);
			spot->GetTransform().Scale = vmath::vec3(0.1f, 0.1f, 0.1f);

			auto *sl = spot->GetComponent<SJH::SpotLight>();
			sl->Distance = 128.0f;
			sl->Ambient  = vmath::vec3(0.0f, 0.0f, 0.0f);
			sl->Diffuse  = vmath::vec3(1.0f, 1.0f, 1.0f);
			sl->Specular = vmath::vec3(1.0f, 1.0f, 1.0f);

			auto *mat = detail::MakeSimpleMat(reg, "mat_marker_spot", progs.simple,
			                                   vmath::vec4(sl->Diffuse, 1.0f));
			spot->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat, QUEUE_OPAQUE);
			dir.Root().AddChild(std::move(spot));
		}
	}
} // namespace MigrateDemo::Scene
