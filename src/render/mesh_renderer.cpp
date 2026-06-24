/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer::Render 자가발행 잎 - 구 MeshPassProcessor WorldMesh draw 로직 이주.
 * @details ROP 적용은 RenderableProcessor::Process 루프 담당(여기서 안 함). FrameBlock 은 per-draw 업로드
 *          (UseProgram 내부 dedup 으로 glUseProgram 만 skip; FrameBlock 멱등 dedup 은 post-profile).
 */
#include "render/mesh_renderer.h"
#include "scene/camera.h"
#include "program/program.h"
#include "object/mesh.h"
#include <glm/glm.hpp>

namespace SJH::Scene
{
	void MeshRenderer::Render(DeviceContext &rec, const Camera &cam) const
	{
		if (!Material || !Mesh) return;
		const SJH::Program *program = Material->GetProgram();
		if (!program) return;

		rec.UseProgram(*program);                          // 내부 dedup (중복 glUseProgram skip)
		if (program->HasUniformBlocks())
		{
			const glm::mat4 view = cam.GetViewMatrix();
			const glm::mat4 proj = cam.GetProjectionMatrix();
			program->UpdateUniformBlock("FrameBlock", &view, sizeof(glm::mat4), 0);
			program->UpdateUniformBlock("FrameBlock", &proj, sizeof(glm::mat4), sizeof(glm::mat4));
		}
		Material->Properties.BindSamplers(rec, *program);
		if (program->HasUniformBlocks())
		{
			Material->Properties.UploadMaterialUboMembers(*program);
			if (Material->Properties.Vec4s.find("baseColor") == Material->Properties.Vec4s.end())
			{
				const glm::vec4 white(1.0f);               // 구 fallback 보존
				program->UpdateUniformMember("baseColor", &white, sizeof(glm::vec4));
			}
			const glm::mat4 model = GetOwner()->GetWorldMatrix();
			program->UpdateUniformBlock("DrawBlock", &model, sizeof(glm::mat4), 0);
			program->BindUniformBlocks();
		}
		// ROP(ApplyRenderStateBlock) 미호출 - RenderableProcessor::Process 가 r->Render 직전 적용.
		rec.BindVAO(Mesh->GetVAO());
		rec.DrawIndexed(Mesh->GetIndexCount());
	}
} // namespace SJH::Scene
