/**
 * @file render_pipeline.cpp
 * @brief SetupDefaultPipeline / BuildPostFXChain 구현.
 *
 * @details
 *  ### 책임
 *  - @c SetupDefaultPipeline - passthrough 셰이더/Mesh/bypass Material 을 ResourceRegistry 에 등록하고,
 *    ScreenQuadStage 를 생성해 반환 (Pure Factory).
 *  - @c BuildPostFXChain - 복수 PostFX 스테이지를 체인 연결:
 *    각 스테이지마다 Program + Material + Framebuffer 생성 -> PassActor + PassComponent 구성 -> screenCamActor 자식 추가.
 *
 *  ### 비-책임
 *  - [X] Stage 수명 관리 / 실행 순서 - Application 의 @c mStages 벡터 책임.
 *  - [X] 생성된 Framebuffer 보유 - @c PostFXChainResult::Framebuffers 를 caller 가 멤버로 보관.
 *
 * @note 두 함수 모두 *실패 시* 해당 자원 skip + @c spdlog::error 출력 후 계속.
 *       @c SetupDefaultPipeline 은 어느 단계든 실패하면 @c nullptr 반환.
 */
#include "render_bootstrap/render_pipeline.h"

#include "object/mesh.h"
#include "material/material.h"
#include "render/pass_component.h"
#include "render/render_stage/render_stage.impls.h"   // SceneRenderer + ScreenQuadStage 통합
#include "resource_registry/resource_registry.h"
#include "scene/actor.h"
#include "scene/layer.h"

#include <spdlog/spdlog.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SJH::Render
{
	std::unique_ptr<ScreenQuadStage> SetupDefaultPipeline(
	    ResourceRegistry& reg,
	    SceneRenderer& sceneRenderer,
	    Framebuffer* sceneFB,
	    const DefaultPipelineConfig& cfg)
	{
		// passthrough Program 등록.
		auto* passthroughProg = reg.CreateProgram(
		    cfg.PassthroughKey,
		    cfg.PassthroughVS,
		    cfg.PassthroughFS);
		if (!passthroughProg)
		{
			spdlog::error("[SetupDefaultPipeline] passthrough 셰이더 로드 실패: {}", cfg.PassthroughFS);
			return nullptr;
		}

		// ScreenQuad Mesh 등록.
		auto* quadMesh = reg.RegisterMesh(cfg.ScreenQuadMeshKey, Mesh::CreateScreenQuad());
		if (!quadMesh)
		{
			spdlog::error("[SetupDefaultPipeline] ScreenQuad Mesh 등록 실패 (중복 키?): {}", cfg.ScreenQuadMeshKey);
			return nullptr;
		}

		// ScreenQuadStage 생성 + 초기 sources = sceneFB.
		auto screenQuadStage = std::make_unique<ScreenQuadStage>(*passthroughProg, *quadMesh);
		screenQuadStage->SetSources({sceneFB}); // 초기 sources fallback

		// bypassMaterial 등록 + SceneRenderer 주입.
		auto* bypassMat = reg.CreateSharedMaterial(cfg.BypassMatKey);
		if (!bypassMat)
		{
			spdlog::error("[SetupDefaultPipeline] bypass Material 생성 실패 (중복 키?): {}", cfg.BypassMatKey);
			return nullptr;
		}
		bypassMat->SetProgram(passthroughProg);

		sceneRenderer.SetScreenQuadMesh(quadMesh);
		sceneRenderer.SetBypassMaterial(bypassMat);

		return screenQuadStage;
	}

	PostFXChainResult BuildPostFXChain(
	    ResourceRegistry& reg,
	    Scene::Actor& screenCamActor,
	    const std::vector<PostFXStageConfig>& configs,
	    Framebuffer* sceneFB,
	    int fbWidth, int fbHeight)
	{
		PostFXChainResult result;
		result.Framebuffers.reserve(configs.size());
		result.PassComponents.reserve(configs.size());

		Framebuffer* prevFB = sceneFB;
		for (const auto& def : configs)
		{
			const auto matKey  = std::string("mat_pass_") + def.Name;

			auto* prog = reg.CreateProgram(def.Name, def.VertFile, def.FragFile);
			if (!prog)
			{
				spdlog::error("[PostFXChain] 셰이더 로드 실패: {}", def.FragFile);
				continue;
			}

			auto* mat = reg.CreateSharedMaterial(matKey);
			if (!mat)
			{
				spdlog::error("[PostFXChain] Material 생성 실패 (중복 키?): {}", matKey);
				continue;
			}
			mat->SetProgram(prog);

			// D-6 data-driven - InitFloats 가 mat->Properties.Floats 로 복사.
			for (const auto& [name, value] : def.InitFloats)
			{
				mat->Properties.Floats[name] = value;
			}

			auto fb = Framebuffer::Create(fbWidth, fbHeight);
			if (!fb)
			{
				spdlog::error("[PostFXChain] FB 생성 실패: {}", def.Name);
				continue;
			}
			auto* fbPtr = fb.get();

			auto passActor = std::make_unique<Scene::Actor>(std::string("PassActor_") + def.Name);
			passActor->SetLayer(Scene::Layer::Screen);
			auto* pc = passActor->AddComponent<Scene::PassComponent>(prevFB, fbPtr, mat);

			result.PassComponents.push_back(pc);
			result.Framebuffers.push_back(std::move(fb));
			prevFB = fbPtr;

			screenCamActor.AddChild(std::move(passActor));
		}

		return result;
	}
}
