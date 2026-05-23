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
 *   - Outline (simple yellow shell) — Box2 자식, scale(1.05) + Stencil(NOTEQUAL, ref=1) + DepthTest off
 *   - Windows × 3 (alpha discard 텍스쳐) — translate, Pass=Transparent (자동 queue 3000 + DepthWrite off + blend on)
 *   - PointLight0 / PointLight1 / SpotLight 마커 큐브 (simple program, scale 0.1)
 *   - DirLight (마커 없음 — 방향만)
 *
 *   ### Compound Actor 컨벤션
 *   - Camera/DirLight/PointLight/SpotLight 는 free factory (compound_actor.h).
 *   - 광원 Actor 에 MeshRenderer 를 추가로 부착 — Light Component 와 type_index 충돌 없음.
 *
 *   ### Pass 컨벤션 (SP-Pass)
 *   - Material 의 `SetPass(Pass::Kind::Transparent)` 한 줄로 queue/blend/depthWrite 자동.
 *   - 챕터/사용자가 *수동으로 `DepthWrite=false` / `QueueLayer=3000`* 설정 *불필요*.
 *   - Material 측 선언이 *진실의 원천* — Unity SurfaceType / Cocos technique 정통.
 *
 *   ### 모자란 부분 (의도된 단순화)
 *   - **Stencil outline** — 레퍼런스는 stencil 마스킹으로 outline. SJH SceneRenderer 은
 *     per-actor GL 상태 변경 미지원이므로 *shell scale 트릭* 으로 근사 (queueLayer ordering).
 *     완벽한 outline 은 SceneRenderer 확장 필요 (TODO).
 *   - **FlashLight 모드 / ImGui 컨트롤** — ImGui 모듈 폐기 상태라 키 조작/슬라이더 생략.
 *   - **Depth func combo** — runtime 변경 UI 없음. GL_LESS (기본) 고정.
 */

#include "GL/gl3w.h"
#include "material/material.h"
#include "material/material_uniforms.h"
#include "material/pass.h"
#include "object/light.h"
#include "object/mesh.h"
#include "program/program.h"
#include "resource_registry/image.h"
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "render/mesh_renderer.h"
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

	/// @brief 워밍업 후 ImGui 컨트롤이 잡아야 하는 핵심 actor/component 포인터.
	/// @details 광원 enable 토글 / FlashLight 모드 / 색상 슬라이더 등을 위해 main.cpp 가 보유.
	struct SceneRefs
	{
		SJH::DirLight   *dirLight       = nullptr;
		SJH::PointLight *pointLights[2] = {nullptr, nullptr};
		SJH::SpotLight  *spotLight      = nullptr;
		SJH::Scene::Actor *spotLightActor = nullptr; // FlashLight 모드 시 카메라 추종.
	};

	// === Queue Layer 컨벤션 (엔진 측 SJH::Pass::Kind underlying value 가 진실의 원천) ===
	// `static_cast<int>(SJH::Pass::Kind::Opaque)`      == 2000
	// `static_cast<int>(SJH::Pass::Kind::AlphaTest)`   == 2450
	// `static_cast<int>(SJH::Pass::Kind::Transparent)` == 3000
	// 일반 Material 은 `mat->SetPass(Pass::Kind::Xxx)` 한 줄로 queue/blend/depth 자동.
	// 챕터 측이 *명시 override* 필요한 케이스만 `Pass::QueueOf(Kind, offset)` 사용.

	struct Programs
	{
		const SJH::Program *phong  = nullptr; // 씬 본체 (Plane / Box1 / Box2) — sampler2D material
		const SJH::Program *simple = nullptr; // 마커 큐브 / 아웃라인 — baseColor 단색
		const SJH::Program *window = nullptr; // 윈도우 — alpha discard
	};

	/// @brief 이미지 -> 텍스쳐 워밍업. ResourceRegistry 에 일괄 등록.
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
		reg.RegisterMesh("mesh_plane",         SJH::Mesh::CreatePlane());
		reg.RegisterMesh("mesh_screen_quad", SJH::Mesh::CreateScreenQuad());
	}

	/// @brief 본체 액터 워밍업 — Plane / Box1 / Box2 (+ Outline) / Windows × 3.
	/// @details Pass::Kind 컨벤션 — Material 측 선언으로 queue/blend/depthWrite 자동.
	///          factory wrapper (Make*Mat) 미사용 — 제너럴 Material::Create + SetProgram + SetXxx + SetPass 패턴.
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

		// --- Plane (marble + gray + shininess 128) — Pass=Opaque (기본) ---
		{
			auto *mat = reg.CreateMaterial("mat_plane");
			mat->SetProgram(progs.phong);
			SJH::Uniforms::SetTexture(*mat, "material.diffuse",  texMarble, 0);
			SJH::Uniforms::SetTexture(*mat, "material.specular", texGray,   1);
			SJH::Uniforms::SetFloat  (*mat, "material.shininess", 128.0f);
			// Pass::Kind::Opaque 가 기본 — 명시 호출 불요.

			auto plane = std::make_unique<SJH::Scene::Actor>("Plane");
			plane->SetLayer(LAYER_SCENE);
			plane->GetTransform().Translate = vmath::vec3(0.0f, -0.5f, 0.0f);
			plane->GetTransform().Scale     = vmath::vec3(10.0f, 1.0f, 10.0f);
			plane->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat);
			dir.Root().AddChild(std::move(plane));
		}

		// --- Box1 (container + dark_gray + shininess 16) — Pass=Opaque ---
		{
			auto *mat = reg.CreateMaterial("mat_box1");
			mat->SetProgram(progs.phong);
			SJH::Uniforms::SetTexture(*mat, "material.diffuse",  texContainer, 0);
			SJH::Uniforms::SetTexture(*mat, "material.specular", texDarkGray,  1);
			SJH::Uniforms::SetFloat  (*mat, "material.shininess", 16.0f);

			auto box = std::make_unique<SJH::Scene::Actor>("Box1");
			box->SetLayer(LAYER_SCENE);
			box->GetTransform().Translate = vmath::vec3(-1.0f, 0.75f, -4.0f);
			box->GetTransform().EulerRot  = vmath::vec3(0.0f, 30.0f, 0.0f);
			box->GetTransform().Scale     = vmath::vec3(1.5f, 1.5f, 1.5f);
			box->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat);
			dir.Root().AddChild(std::move(box));
		}

		// --- Box2 + Outline child (stencil 정통) — Pass=Opaque ---
		// 레퍼런스 (context.cpp): Box2 를 stencil=1 로 도장 후, Outline 셸 (scale 1.05) 을
		// stencil!=1 + depth off 로 그려 rim 만 노출.
		{
			auto *matBox2 = reg.CreateMaterial("mat_box2");
			matBox2->SetProgram(progs.phong);
			SJH::Uniforms::SetTexture(*matBox2, "material.diffuse",  texContainer2,     0);
			SJH::Uniforms::SetTexture(*matBox2, "material.specular", texContainer2Spec, 1);
			SJH::Uniforms::SetFloat  (*matBox2, "material.shininess", 64.0f);

			auto box = std::make_unique<SJH::Scene::Actor>("Box2");
			box->SetLayer(LAYER_SCENE);
			box->GetTransform().Translate = vmath::vec3(0.0f, 0.75f, 2.0f);
			box->GetTransform().EulerRot  = vmath::vec3(0.0f, 20.0f, 0.0f);
			box->GetTransform().Scale     = vmath::vec3(1.5f, 1.5f, 1.5f);
			auto *mrBox2 = box->AddComponent<SJH::Scene::MeshRenderer>(meshBox, matBox2);
			// stencil=1 도장 — 어느 픽셀이 Box2 내부인지 mark.
			mrBox2->Stencil.Enabled   = true;
			mrBox2->Stencil.Func      = GL_ALWAYS;
			mrBox2->Stencil.Ref       = 1;
			mrBox2->Stencil.TestMask  = 0xFFu;
			mrBox2->Stencil.SFail     = GL_KEEP;
			mrBox2->Stencil.DpFail    = GL_KEEP;
			mrBox2->Stencil.DpPass    = GL_REPLACE; // depth+stencil pass 시 stencil 에 ref(1) 쓰기.
			mrBox2->Stencil.WriteMask = 0xFFu;

			// Outline 셸 — Box2 의 자식. queueLayer override (Box2 이후 그리도록 2005).
			auto *matOutline = reg.CreateMaterial("mat_outline");
			matOutline->SetProgram(progs.simple);
			SJH::Uniforms::SetVec4(*matOutline, "baseColor", vmath::vec4(1.0f, 1.0f, 0.5f, 1.0f));

			auto outline = std::make_unique<SJH::Scene::Actor>("Outline");
			outline->SetLayer(LAYER_SCENE);
			outline->GetTransform().Scale = vmath::vec3(1.05f, 1.05f, 1.05f);
			// Outline 은 Box2 (Opaque, queue 2000) *직후* 그려야 stencil 마스킹 의도.
			// matOutline 의 PassKind=Opaque (기본) 라 base queue 가 2000.
			// -> MeshRenderer.QueueOffset = +5 -> 최종 queue 2005 — Unity Renderer.sortingOrder 정통.
			auto *mrOutline = outline->AddComponent<SJH::Scene::MeshRenderer>(
			    meshBox, matOutline, /*queueOffset*/ 5);
			// Outline 픽셀은 stencil!=1 일 때만 그림 (Box2 가 도장한 안쪽은 skip).
			mrOutline->Stencil.Enabled   = true;
			mrOutline->Stencil.Func      = GL_NOTEQUAL;
			mrOutline->Stencil.Ref       = 1;
			mrOutline->Stencil.TestMask  = 0xFFu;
			mrOutline->Stencil.SFail     = GL_KEEP;
			mrOutline->Stencil.DpFail    = GL_KEEP;
			mrOutline->Stencil.DpPass    = GL_KEEP;
			mrOutline->Stencil.WriteMask = 0x00u; // stencil 에 쓰지 않음 (read-only).
			// depth 비활성 — Outline 이 다른 오브젝트 뒤에 있어도 표면 그려지도록. AND 합성으로 *false 가 우선*.
			mrOutline->DepthTest  = false;
			mrOutline->DepthWrite = false;
			box->AddChild(std::move(outline));

			dir.Root().AddChild(std::move(box));
		}

		// --- Windows × 3 (alpha discard) — Pass=Transparent (자동 queue 3000 + DepthWrite off + blend on) ---
		{
			auto *matWindow = reg.CreateMaterial("mat_window");
			matWindow->SetProgram(progs.window);
			SJH::Uniforms::SetTexture(*matWindow, "tex0", texWindow, 0);
			matWindow->SetPass(SJH::Pass::Kind::Transparent);   // ★ 한 줄로 모든 state 자동

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
				// QueueLayer / DepthWrite 수동 설정 *불필요* — Pass=Transparent 가 자동.
				win->AddComponent<SJH::Scene::MeshRenderer>(meshPlane, matWindow);
				dir.Root().AddChild(std::move(win));
			}
		}
	}

	/// @brief 광원 워밍업 — DirLight + PointLight × 2 + SpotLight + 각 점/스포트 광원 마커 큐브.
	/// @return SceneRefs — main.cpp 의 ImGui 컨트롤이 잡아야 하는 component/actor 포인터.
	inline SceneRefs WarmupLights(const Programs &progs, SJH::Scene::Director &dir)
	{
		auto &reg     = SJH::ResourceRegistry::Get();
		auto *meshBox = reg.FindMesh("mesh_box");

		SceneRefs refs;

		// === DirLight (마커 없음 — 평행광이라 위치 무의미) ===
		{
			auto sun = SJH::Scene::CreateDirLightActor("Sun", vmath::vec3(0.0f, -1.0f, 0.0f));
			sun->SetLayer(LAYER_SCENE);
			auto *dl = sun->GetComponent<SJH::DirLight>();
			dl->Ambient  = vmath::vec3(1.0f, 1.0f, 1.0f);
			dl->Diffuse  = vmath::vec3(1.0f, 1.0f, 1.0f);
			dl->Specular = vmath::vec3(1.0f, 1.0f, 1.0f);
			refs.dirLight = dl;
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

			auto *mat = reg.CreateMaterial("mat_marker_lamp0");
			mat->SetProgram(progs.simple);
			SJH::Uniforms::SetVec4(*mat, "baseColor", vmath::vec4(pl->Diffuse, 1.0f));

			lamp->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat);
			refs.pointLights[0] = pl;
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

			auto *mat = reg.CreateMaterial("mat_marker_lamp1");
			mat->SetProgram(progs.simple);
			SJH::Uniforms::SetVec4(*mat, "baseColor", vmath::vec4(pl->Diffuse, 1.0f));

			lamp->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat);
			refs.pointLights[1] = pl;
			dir.Root().AddChild(std::move(lamp));
		}

		// === SpotLight — translate(1, 4, 4), 아래 방향, cutoff 5°/120° ===
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

			auto *mat = reg.CreateMaterial("mat_marker_spot");
			mat->SetProgram(progs.simple);
			SJH::Uniforms::SetVec4(*mat, "baseColor", vmath::vec4(sl->Diffuse, 1.0f));

			spot->AddComponent<SJH::Scene::MeshRenderer>(meshBox, mat);
			refs.spotLight      = sl;
			refs.spotLightActor = spot.get();
			dir.Root().AddChild(std::move(spot));
		}

		return refs;
	}
} // namespace MigrateDemo::Scene
