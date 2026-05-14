#ifndef __ENGINE_MODEL_BASE_H__
#define __ENGINE_MODEL_BASE_H__

#include "GL/gl3w.h"
#include "engine/material.h"
#include "engine/transform.h"
#include <vector>

namespace Engine::Model
{
	class ModelBase
	{
	  protected:
		GLuint mVAOAddr;
		GLuint mVBOAddr;
		GLuint mEBOAddr;

		std::vector<GLfloat> mBufferData;
		std::vector<GLuint> mElementData;
		GLuint mIndexCount;

		bool isBuilted = false;

		Transform::Transform mTransform;
		// Material 은 외부 소유. ModelBase 는 비소유 포인터로 참조만 한다.
		// nullptr 이면 Draw 가 BaseColor=white + 모든 텍스처 슬롯 비활성으로 그린다.
		Material::Material *mMaterial = nullptr;

	  public:
		Transform::Transform &GetTransform()
		{
			return mTransform;
		}

		// Material 약결합 — 소유권은 호출 측. SetMaterial 호출 없이도 Draw 가능 (디폴트 머티리얼).
		void SetMaterial(Material::Material *m)
		{
			mMaterial = m;
		}
		Material::Material *GetMaterial() const
		{
			return mMaterial;
		}

		GLuint GetVAOAddr() const
		{
			return mVAOAddr;
		}
		GLuint GetIndexCount() const
		{
			return mIndexCount;
		}

		ModelBase();
		virtual ~ModelBase();

		void Deconstruct();
		void Build(const std::vector<GLfloat> &buffer_data);

		// material->program 을 사용해서 그림. material 또는 program 이 nullptr 이면 그리지 않음.
		// 전제: 호출자가 material->program->Apply(view, proj, viewPos) 를 매 프레임 한 번 호출했음.
		void Draw() const;

		// PhongMaterial 의 diffuse/specular 텍스처를 각자 지정된 image unit 에 바인딩.
		// (multi_lighting.h 의 chapter9 BindMaterialTextures 를 ModelBase 정적 헬퍼로 흡수)
		static void BindPhongMaterial(const Material::PhongMaterial &material);
	};
} // namespace Engine::Model

#endif // __ENGINE_MODEL_BASE_H__
