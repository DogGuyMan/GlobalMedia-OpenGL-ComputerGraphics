#include "engine/model_base.h"
#include "diagnostics/engine_diagnostics.h"
#include "diagnostics/gl_log.h"
#include "engine/constants.h"
#include "engine/geometry.h"
#include "engine/shader_program.h" // mMaterial->program->ProgAddr 사용

namespace diag = SJH::Diagnostics;

namespace Engine::Model
{
	ModelBase::ModelBase() = default;

	ModelBase::~ModelBase()
	{
		Deconstruct();
	}

	void ModelBase::Deconstruct()
	{
		if (!isBuilted)
			return;
		glDeleteBuffers(1, &mEBOAddr);
		glDeleteBuffers(1, &mVBOAddr);
		glDeleteVertexArrays(1, &mVAOAddr);
		isBuilted = false;
	}

	void ModelBase::Build(const std::vector<GLfloat> &buffer_data)
	{
		if (isBuilted)
			return;
		diag::EngineDiagnostics::CheckInterleavedVertexBuffer(
		    buffer_data, Constants::GEOMETRY::VERTEX_LEN,
		    0, Constants::GEOMETRY::VERTEX_POSITION_SIZE,
		    Constants::GEOMETRY::VERTEX_POSITION_SIZE + Constants::GEOMETRY::VERTEX_COLOR_SIZE, Constants::GEOMETRY::VERTEX_NORMAL_SIZE);
		mBufferData = std::vector<GLfloat>(buffer_data);

		mIndexCount = mBufferData.size() / Constants::GEOMETRY::VERTEX_LEN;
		for (GLuint i = 0; i < mIndexCount; i++)
			mElementData.push_back(i);

		glGenVertexArrays(1, &mVAOAddr);
		diag::GLDebug::CheckGLGenVertexArrays();
		glBindVertexArray(mVAOAddr);
		diag::GLDebug::CheckGLBindVertexArray(mVAOAddr);

		glGenBuffers(1, &mVBOAddr);
		diag::GLDebug::CheckGLGenBuffers(mVBOAddr);
		glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
		diag::GLDebug::CheckGLBindBuffer(mVBOAddr);
		glBufferData(GL_ARRAY_BUFFER,
		             mBufferData.size() * sizeof(GLfloat),
		             mBufferData.data(), GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(mBufferData.size() * sizeof(GLfloat)));

		glGenBuffers(1, &mEBOAddr);
		diag::GLDebug::CheckGLGenBuffers(mEBOAddr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
		diag::GLDebug::CheckGLBindBuffer(mEBOAddr);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,
		             mElementData.size() * sizeof(GLuint),
		             mElementData.data(), GL_STATIC_DRAW);
		diag::GLDebug::CheckGLBufferData(static_cast<GLint>(mElementData.size() * sizeof(GLuint)));

		GLuint stride = Constants::GEOMETRY::VERTEX_LEN * sizeof(GLfloat);
		void *poffset = (void *)0;
		void *coffset = (void *)(Constants::GEOMETRY::VERTEX_POSITION_SIZE * sizeof(GLfloat));
		void *noffset = (void *)((Constants::GEOMETRY::VERTEX_POSITION_SIZE + Constants::GEOMETRY::VERTEX_COLOR_SIZE) * sizeof(GLfloat));
		void *uvoffset = (void *)((Constants::GEOMETRY::VERTEX_POSITION_SIZE + Constants::GEOMETRY::VERTEX_COLOR_SIZE + Constants::GEOMETRY::VERTEX_NORMAL_SIZE) * sizeof(GLfloat));

		glVertexAttribPointer(0, Constants::GEOMETRY::VERTEX_POSITION_SIZE, GL_FLOAT, false, stride, poffset);
		diag::GLDebug::CheckGLVertexAttribPointer({static_cast<GLsizei>(stride)});
		glEnableVertexAttribArray(0);
		diag::GLDebug::CheckGLEnableVertexAttribArray(0);
		glVertexAttribPointer(1, Constants::GEOMETRY::VERTEX_COLOR_SIZE, GL_FLOAT, false, stride, coffset);
		diag::GLDebug::CheckGLVertexAttribPointer({static_cast<GLsizei>(stride)});
		glEnableVertexAttribArray(1);
		diag::GLDebug::CheckGLEnableVertexAttribArray(1);
		glVertexAttribPointer(2, Constants::GEOMETRY::VERTEX_NORMAL_SIZE, GL_FLOAT, false, stride, noffset);
		diag::GLDebug::CheckGLVertexAttribPointer({static_cast<GLsizei>(stride)});
		glEnableVertexAttribArray(2);
		diag::GLDebug::CheckGLEnableVertexAttribArray(2);
		glVertexAttribPointer(3, Constants::GEOMETRY::VERTEX_UV_SIZE, GL_FLOAT, false, stride, uvoffset);
		diag::GLDebug::CheckGLVertexAttribPointer({static_cast<GLsizei>(stride)});
		glEnableVertexAttribArray(3);
		diag::GLDebug::CheckGLEnableVertexAttribArray(3);
		isBuilted = true;
	}

	void ModelBase::Draw() const
	{
		// Material 또는 program 미설정 → 그릴 progAddr 없음 → skip.
		if (mMaterial == nullptr || mMaterial->program == nullptr)
			return;

		const GLuint progAddr = mMaterial->program->ProgAddr;

		// 모델 단위 uniform (transform) + 머티리얼 단위 uniform (BaseColor, textures)
		glUniformMatrix4fv(glGetUniformLocation(progAddr, Engine::Constants::UNIFORM::UNIFORM_MODEL_MAT),
		                   1, false, mTransform.GetModelMatrix());
		mMaterial->Apply();

		glBindVertexArray(mVAOAddr);
		glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
	}

	// PhongMaterial 의 diffuse/specular 텍스처를 각자 정해진 image unit 에 바인딩.
	// 셰이더 측 sampler2D uniform 은 Engine::Uniforms::UniformsSetMaterial 로 unit 번호를 받음.
	void ModelBase::BindPhongMaterial(const Material::PhongMaterial &material)
	{
		glActiveTexture(GL_TEXTURE0 + material.diffuseUnit);
		glBindTexture(GL_TEXTURE_2D, material.diffuseTexture);
		glActiveTexture(GL_TEXTURE0 + material.specularUnit);
		glBindTexture(GL_TEXTURE_2D, material.specularTexture);
	}
} // namespace Engine::Model
