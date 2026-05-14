#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "vmath.h"
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sb7.h>
#include <shader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "diagnostics/engine_diagnostics.h"
#include "diagnostics/gl_log.h"
#include "diagnostics/uniform_diagnostics.h"

#include "engine/camera.h"
#include "engine/constants.h"
#include "engine/geometry.h"
#include "engine/lighting.h"
#include "engine/material.h"
#include "engine/model_base.h"
#include "engine/resource_management.h"
#include "engine/scene_graph.h"
#include "engine/shader_program.h"
#include "engine/transform.h"
#include "engine/uniforms.h"

using namespace std;
using namespace vmath;
namespace diag = SJH::Diagnostics;

// ───────────────────────────────────────────────────────────────────────
// 본 파일은 chapter 별로 달라지는 "고변동성" 영역만 보유한다:
//   Engine::Context   — sceneGraph / resourceManagement 인스턴스 + 렌더 상태
//                       (camera, program, dummy*) + ClearBuffer / TeardownGL
//   chapter7          — MyApplication (sb7 라이프사이클 + 씬 셋업)
//
// SceneGraph / ResourceManagement 클래스 본체는 src/engine/ 으로 이동.
// engine/scene_graph.h, engine/resource_management.h 참고.
// Engine::Program (ShaderProgram base + Default/Texture) 도 엔진 라이브러리에서 제공.
// ───────────────────────────────────────────────────────────────────────

namespace Engine::Context
{
	// 두 모듈의 인스턴스를 Context 가 보유. 향후 multi-scene 확장 시 인스턴스 추가만으로 처리.
	Engine::SceneGraph::SceneGraph sceneGraph;
	Engine::ResourceManagement::ResourceManagement resourceManagement;

	// 렌더링 상태 단일 책임 — 카메라 / 현재 프로그램 / 디버그 더미.
	Camera::Camera main_camera;
	// Light 참조는 TextureShaderProgram::AttachedLight 멤버로 이동 (셰이더 의존 데이터).
	std::unique_ptr<Program::TextureShaderProgram> program;
	std::unique_ptr<Program::ShaderProgram> dummyProgram;
	GLuint dummyVAO = 0;


	// GL 컨텍스트 살아있는 동안 호출. program → ResourceManagement → SceneGraph → dummyVAO 순.
	void TeardownGL()
	{
		program.reset();
		dummyProgram.reset();
		resourceManagement.TeardownGL();
		sceneGraph.Clear();
		if (dummyVAO != 0)
		{
			glDeleteVertexArrays(1, &dummyVAO);
			dummyVAO = 0;
		}
	}
} // namespace Engine::Context

namespace chapter7
{
	using namespace Engine;

	// chapter7 전용 자원 경로 — 다른 chapter 는 자기 자원을 따로 들고 있을 것.
	static const char *wallTexture = "./textures/wall.jpg";

	class MyApplication : public sb7::application
	{
	  protected:
		void UpdateMembers(double currentTime)
		{
			// 큐브를 y 축 기준으로 천천히 회전
			if (auto *m = Engine::Context::resourceManagement.GetModel("cube"))
				m->GetTransform().EulerRot =
				    vmath::vec3(0.0f, (float)(currentTime * 30.0), 0.0f);
		}

	  public:
		virtual void startup() override
		{
			stbi_set_flip_vertically_on_load(true);
			Engine::Context::dummyProgram = std::make_unique<Program::DefaultShaderProgram>(
			    "./shaders/dummy_vs.glsl",
			    "./shaders/dummy_fs.glsl");
			glGenVertexArrays(1, &Engine::Context::dummyVAO);
			glBindVertexArray(Engine::Context::dummyVAO);

			Engine::Context::program = make_unique<Program::TextureShaderProgram>(
			    "./shaders/default_vs.glsl",
			    "./shaders/texture_fs.glsl");

			// 카메라: 원점을 바라보는 위치
			Engine::Context::main_camera.Transform.Translate = vmath::vec3(2.0f, 2.0f, 3.0f);
			Engine::Context::main_camera.Target = vmath::vec3(0.0f, 0.0f, 0.0f);

			// 월드 루트 + 텍스처 큐브 한 개 등록
			auto *root = Engine::Context::sceneGraph.CreateRootTransform(
			    Engine::Context::sceneGraph.name_of_WorldRoot_transform);

			auto cube = std::make_unique<Model::ModelBase>();
			std::vector<GLfloat> data;
			Model::BuildCube(data);
			cube->Build(data);

			// Material 은 ResourceManagement 가 소유. Model 은 raw 포인터로 참조만.
			// Material → Program 단방향 의존: 어떤 셰이더로 그릴지 Material 이 안다.
			auto *wallMat = Engine::Context::resourceManagement.AddMaterial(
			    "wall",
			    std::make_unique<Engine::Material::Material>());
			wallMat->program = Engine::Context::program.get();
			wallMat->LoadTexture(wallTexture);
			cube->SetMaterial(wallMat);

			Engine::Context::resourceManagement.AddModel("cube", std::move(cube), root);
		}

		virtual void render(double currentTime) override
		{
			Engine::Context::program->ClearBuffer();
			Engine::Context::main_camera.Aspect = ((float)info.windowWidth) / info.windowHeight;
			UpdateMembers(currentTime);

			if (Engine::Context::program)
				Engine::Context::program->Render(
				    Engine::Context::resourceManagement.Models,
				    Engine::Context::main_camera.GetViewMatrix(),
				    Engine::Context::main_camera.GetProjMatrix(),
				    Engine::Context::main_camera.Transform.Translate);
		}

		virtual void shutdown() override
		{
			Engine::Context::TeardownGL();
		}
	};
}; // namespace chapter7

DECLARE_MAIN(chapter7::MyApplication);
