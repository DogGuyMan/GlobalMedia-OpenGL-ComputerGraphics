#ifndef __ENGINE_MATERIAL_H__
#define __ENGINE_MATERIAL_H__

#include "GL/gl3w.h"
#include "vmath.h"
#include <vector>

// Material 이 program* 을 들기 위한 forward declaration.
// Material → Program 단방향 의존 (Material 은 어떤 셰이더로 그릴지 안다).
namespace Engine::Program
{
	class ShaderProgram;
}

namespace Engine::Material
{
	struct TextureSlot
	{
		GLuint TexAddr;
		vmath::vec2 UVOffset;
		vmath::vec2 UVRatio;

		TextureSlot()
		    : TexAddr(0),
		      UVOffset(vmath::vec2(0.0f, 0.0f)),
		      UVRatio(vmath::vec2(1.0f, 1.0f))
		{
		}
	};

	struct TextureParams
	{
		GLint WrapS = GL_REPEAT;
		GLint WrapT = GL_REPEAT;
		GLint MinFilter = GL_LINEAR_MIPMAP_LINEAR;
		GLint MagFilter = GL_LINEAR;
	};

	// Base — chapter7 셰이더(texture_fs.glsl) 가 사용하는 다중 슬롯 텍스처 머티리얼.
	// shader-dependent 정보(diffuse/specular map 핸들 + shininess 등) 를 갖는
	// 머티리얼은 이 Base 를 상속한 specific 구체 클래스로 분리.
	class Material
	{
	  public:
		// 어떤 셰이더 프로그램으로 그릴지. 외부 소유 (예: Context::program).
		// nullptr 이면 Apply() 가 no-op 이고 Model::Draw() 도 그리지 못한다.
		Engine::Program::ShaderProgram *program = nullptr;

		vmath::vec4 BaseColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		std::vector<TextureSlot> Slots;

		Material();
		virtual ~Material();

		// Model 에 결합된 값 타입. 복사 금지 — 같은 GPU 텍스처 핸들이 여러 Material 에
		// 공유되면 소멸 시 double-free 가 발생하기 때문.
		Material(const Material &) = delete;
		Material &operator=(const Material &) = delete;

		void LoadTexture(const char *image_path,
		                 GLuint format = GL_RGB,
		                 GLint internal_format = GL_RGB,
		                 const TextureParams &params = TextureParams{});

		int GetSlotCount() const
		{
			return (int)Slots.size();
		}

		// Material 의 셰이더 uniform (BaseColor + texture slots) 을 program 에 셋업.
		// 전제: 호출 전에 program->Apply(view, proj, viewPos) 가 한 프레임에 한 번 호출되어
		//       glUseProgram 이 끝나 있어야 한다. program==nullptr 이면 no-op.
		void Apply() const;
	};

	// Phong / Blinn-Phong 셰이더 (chapter9 basic_lighting_fs.glsl) 계약에 맞춘 specific Material.
	// shader 에서 `material.diffuse` (sampler2D), `material.specular` (sampler2D), `material.shininess`
	// uniform 을 읽으므로 그에 대응하는 핸들/유닛 번호/지수를 멤버로 둠.
	// Base 의 BaseColor / Slots 도 상속받지만 이 셰이더에서는 Slots 를 사용하지 않는다 (비어둠).
	class PhongMaterial : public Material
	{
	  public:
		GLuint diffuseTexture = 0;  // diffuse map 텍스처 객체 핸들
		GLuint specularTexture = 0; // specular map 텍스처 객체 핸들
		GLint diffuseUnit = 0;      // diffuseTexture 를 바인딩할 텍스처 이미지 유닛 번호 (sampler2D 에 넣는 값)
		GLint specularUnit = 1;     // specularTexture 를 바인딩할 텍스처 이미지 유닛 번호
		float shininess = 32.0f;
	};
} // namespace Engine::Material

#endif // __ENGINE_MATERIAL_H__
