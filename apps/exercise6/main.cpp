#include "GL/gl3w.h"
#include "GL/glcorearb.h"
#include "vmath.h"
#include <iostream>
#include <memory>
#include <sb7.h>
#include <shader.h>
#include <utility>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;
using namespace vmath;

namespace exercise6
{

	static const int VERTEX_POSITION_SIZE = 4;
	static const int VERTEX_COLOR_SIZE = 4;
	static const int VERTEX_UV_SIZE = 2;
	static constexpr int VERTEX_LEN = VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE + VERTEX_UV_SIZE;

	static const char *SHADER_VS_PATH = "./shaders/default_vs.glsl";
	static const char *SHADER_FS_PATH = "./shaders/default_fs.glsl";


	static const char *UNIFORM_MODEL_MAT = "modelMat";
	static const char *UNIFORM_VIEW_MAT = "viewMat";
	static const char *UNIFORM_PROJ_MAT = "projMat";


	static const char *SAMPLER_TEX1 = "tex1";
	static const char *SAMPLER_TEX2 = "tex2";


	static const char *TEXTURE_CONTAINER = "./textures/container.jpg";
	static const char *TEXTURE_SIDES[6] = {
	    "./textures/side1.jpg",
	    "./textures/side2.jpg",
	    "./textures/side3.jpg",
	    "./textures/side4.jpg",
	    "./textures/side5.jpg",
	    "./textures/side6.jpg",
	};
	static const vmath::vec4 CUBE_BASE_POSITIONS[2][4] = {
	    {
	        {0.0, 0.0, 0.0, 1.0},
	        {1.0, 0.0, 0.0, 1.0},
	        {1.0, 0.0, 1.0, 1.0},
	        {0.0, 0.0, 1.0, 1.0},
	    },
	    {
	        {0.0, 1.0, 0.0, 1.0},
	        {1.0, 1.0, 0.0, 1.0},
	        {1.0, 1.0, 1.0, 1.0},
	        {0.0, 1.0, 1.0, 1.0},
	    }};

	static const std::vector<GLuint> CUBE_FACE_INDICES[6] = {
	    {1, 0, 4, 1, 4, 5}, // -Z
	    {2, 1, 5, 2, 5, 6}, // +X
	    {3, 2, 6, 3, 6, 7}, // +Z
	    {0, 3, 7, 0, 7, 4}, // -X
	    {0, 1, 2, 0, 2, 3}, // -Y
	    {7, 6, 5, 7, 5, 4}, // +Y
	};

	static const std::vector<vmath::vec2> BASE_MESH_UVS{
	    {0.0, 0.0},
	    {1.0, 0.0},
	    {1.0, 1.0},
	    {0.0, 1.0}};

	static const vmath::vec4 BASE_COLORS[7]{
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 1.0, 1.0)};

	// 면마다 6 정점 unrolled — 총 36 정점, 면별 독립 UV
	//   CUBE_FACE_INDICES[f] 의 6 인덱스 → CUBE_BASE_POSITIONS lookup → 면마다 정점 복제
	//   UV 는 면 내부 {0,1,2,0,2,3} 패턴으로 BASE_MESH_UVS 4 코너 재사용 → 각 면이 (0,0)~(1,1) 완전 매핑
	inline void BuildCube(vector<GLfloat> &vertices, const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		const int uvIdx[6] = {0, 1, 2, 0, 2, 3};
		const vmath::vec4 *cubeVertices = &CUBE_BASE_POSITIONS[0][0];
		for (int f = 0; f < 6; f++)
		{
			const auto &faceIdx = CUBE_FACE_INDICES[f];
			for (int i = 0; i < 6; i++)
			{
				const auto &pos = cubeVertices[faceIdx[i]];
				const auto &uv = BASE_MESH_UVS[uvIdx[i]];
				vertices.push_back(pos[0] + offset[0]);
				vertices.push_back(pos[1] + offset[1]);
				vertices.push_back(pos[2] + offset[2]);
				vertices.push_back(pos[3]);
				for (int c = 0; c < 4; c++)
					vertices.push_back(BASE_COLORS[f][c]);
				vertices.push_back(uv[0]);
				vertices.push_back(uv[1]);
			}
		}
	}

	static const vmath::vec4 BG_COLOR = vmath::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	class ModelBase;

	class ProgramBase
	{
	  private:
		GLuint mProgramAddr;

		GLuint createShader(GLenum shader_type, const char *shader_path)
		{
			GLuint shaderAddr = sb7::shader::load(shader_path, shader_type, false);
			return shaderAddr;
		};

	  public:
		ProgramBase(const char *VS_PATH, const char *FS_PATH)
		{
			mProgramAddr = glCreateProgram();
			auto vsAddr = createShader(GL_VERTEX_SHADER, VS_PATH);
			auto fsAddr = createShader(GL_FRAGMENT_SHADER, FS_PATH);

			glAttachShader(mProgramAddr, vsAddr);
			glAttachShader(mProgramAddr, fsAddr);

			glLinkProgram(mProgramAddr);

			glDeleteShader(vsAddr);
			glDeleteShader(fsAddr);
		}
		ProgramBase()
		    : ProgramBase(SHADER_VS_PATH, SHADER_FS_PATH)
		{
		}

		~ProgramBase()
		{
			glDeleteProgram(mProgramAddr);
		}

		GLuint GetProgramAddr() const
		{
			return mProgramAddr;
		}
	};

	class ModelBase
	{
	  private:
		GLuint mVAOAddr;
		GLuint mVBOAddr;
		GLuint mEBOAddr;
		vector<GLuint> mTextureAddrs;

		vector<GLfloat> mBufferData;
		vector<GLuint> mElementData;
		GLuint mIndexCount;

	  public:
		vec3 mTranslate = vec3(0.0f, 0.0f, 0.0f);
		vec3 mEulerRot = vec3(0.0f, 0.0f, 0.0f);
		vec3 mScale = vec3(1.0f, 1.0f, 1.0f);

		ModelBase()
		{
		}

		~ModelBase()
		{
			glDeleteBuffers(1, &mEBOAddr);
			glDeleteBuffers(1, &mVBOAddr);
			glDeleteVertexArrays(1, &mVAOAddr);
		}

		void Build(const vector<GLfloat> &buffer_data)
		{
			mBufferData = vector<GLfloat>(buffer_data);
			mIndexCount = mBufferData.size() / VERTEX_LEN;
			for (GLuint i = 0; i < mIndexCount; i++)
				mElementData.push_back(i);

			glGenVertexArrays(1, &mVAOAddr);
			glBindVertexArray(mVAOAddr);

			glGenBuffers(1, &mVBOAddr);
			glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
			glBufferData(GL_ARRAY_BUFFER, mBufferData.size() * sizeof(GLfloat), mBufferData.data(), GL_STATIC_DRAW);

			glGenBuffers(1, &mEBOAddr);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mElementData.size() * sizeof(GLuint), mElementData.data(), GL_STATIC_DRAW);

			GLuint stride = VERTEX_LEN * sizeof(GLfloat);
			void *poffset = (void *)0;
			void *coffset = (void *)(VERTEX_POSITION_SIZE * sizeof(GLfloat));
			void *uvoffset = (void *)((VERTEX_POSITION_SIZE + VERTEX_COLOR_SIZE) * sizeof(GLfloat));

			glVertexAttribPointer(0, VERTEX_POSITION_SIZE, GL_FLOAT, false, stride, poffset);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(1, VERTEX_COLOR_SIZE, GL_FLOAT, false, stride, coffset);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(2, VERTEX_UV_SIZE, GL_FLOAT, false, stride, uvoffset);
			glEnableVertexAttribArray(2);
		}

		mat4 GetModelMatrix()
		{
			return translate(mTranslate) * vmath::rotate<float>(mEulerRot[2], 0.0, 0.0, 1.0) * vmath::rotate<float>(mEulerRot[1], 0.0, 1.0, 0.0) * vmath::rotate<float>(mEulerRot[0], 1.0, 0.0, 0.0) * vmath::scale<float>(mScale);
		}

		void Draw(GLuint prog_addr)
		{
			glBindVertexArray(mVAOAddr);
			glUniformMatrix4fv(glGetUniformLocation(prog_addr, UNIFORM_MODEL_MAT),
			                   1, false, GetModelMatrix());


			glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX1), 0);
			glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX2), 1);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);

			for (int f = 0; f < 6; f++)
			{
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(f * 6 * sizeof(GLuint)));
			}
		}

		void AddTexture(const char *image_path)
		{
			GLuint texture;
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);

			int width, height, nrChannels;
			unsigned char *data = stbi_load(image_path, &width, &height, &nrChannels, 0);
			if (data)
			{
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
				glGenerateMipmap(GL_TEXTURE_2D);
			}
			stbi_image_free(data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			mTextureAddrs.push_back(texture);
		}
	};

	class MyApplication : public sb7::application
	{
		unique_ptr<ProgramBase> program;
		vector<unique_ptr<ModelBase>> models;

		vec3 eye = vec3(0.0, 1.0, 3.0);
		vec3 target = vec3(0.0, 0.0, 0.0);
		vec3 worldup = vec3(0.0, 1.0, 0.0);

		float fov = 60;
		float aspect = 0;
		float nearplane = 0.1;
		float farplane = 1000.0;

		virtual void startup() override
		{
			stbi_set_flip_vertically_on_load(true);
			program = std::make_unique<ProgramBase>();

			vector<GLfloat> vertices;
			BuildCube(vertices);
			auto model = std::make_unique<ModelBase>();
			model->Build(vertices);

			model->AddTexture(TEXTURE_CONTAINER);
			for (int f = 0; f < 6; f++)
				model->AddTexture(TEXTURE_SIDES[f]);

			model->mScale = vec3(0.75, 0.75, 0.75);
			models.push_back(std::move(model));

		};

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR, 0, BG_COLOR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);

			aspect = ((float)info.windowWidth) / info.windowHeight;

			float angle = vmath::radians((currentTime * 180) / M_PI) * 90;

			glUseProgram(program->GetProgramAddr());
			glUniformMatrix4fv(
			    glGetUniformLocation(program->GetProgramAddr(), UNIFORM_VIEW_MAT),
			    1, false, vmath::lookat(eye, target, worldup));
			glUniformMatrix4fv(
			    glGetUniformLocation(program->GetProgramAddr(), UNIFORM_PROJ_MAT),
			    1, false, vmath::perspective(fov, aspect, nearplane, farplane));

			models.back()->mTranslate = vmath::vec3(cosf(currentTime), 0.0, 0.0);
			models.back()->mEulerRot = vmath::vec3(angle, angle, angle);

			for (auto &model : models)
			{
				model->Draw(program->GetProgramAddr());
			}
		}

		virtual void shutdown() override
		{
		}
	};
}; // namespace exercise6

DECLARE_MAIN(exercise6::MyApplication);