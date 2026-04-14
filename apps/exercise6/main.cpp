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
	    {0, 1, 5, 0, 5, 4}, // -Z
	    {1, 2, 6, 1, 6, 5}, // +X
	    {2, 3, 7, 2, 7, 6}, // +Z
	    {3, 0, 4, 3, 4, 7}, // -X
	    {0, 1, 2, 0, 2, 3}, // -Y
	    {4, 5, 6, 4, 6, 7}, // +Y
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

	inline void BuildCube(vector<GLfloat> &vertices, const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{

		const int uvIdx[6] = {0, 1, 2, 0, 2, 3};
		const vmath::vec4 *cubeVertices = &CUBE_BASE_POSITIONS[0][0];
		for (int f = 0; f < 6; f++)
		{
			const auto &faceIdx = CUBE_FACE_INDICES[f]; // 8정점 공간 면 인덱스
			const auto &color = BASE_COLORS[f];         // 면별 색 
			for (int i = 0; i < 6; i++)
			{
				const auto &pos = cubeVertices[faceIdx[i]]; // i 로 인덱싱 (f 아님)
				const auto &uv = BASE_MESH_UVS[uvIdx[i]];
				vertices.push_back(pos[0] + offset[0]);
				vertices.push_back(pos[1] + offset[1]);
				vertices.push_back(pos[2] + offset[2]);
				vertices.push_back(pos[3]); // w 유지
				for (int c = 0; c < 4; c++)
					vertices.push_back(color[c]);
				for (int a = 0; a < 2; a++)
					vertices.push_back(uv[a]);
			}
		}
	}

	static const int VERTEX_POSITION_SIZE = 4;
	static const int VERTEX_COLOR_SIZE = 4;
	static const int VERTEX_UV_SIZE = 2;
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
		    : ProgramBase("./shaders/default_vs.glsl", "./shaders/default_fs.glsl")
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
		vector<pair<const char*, GLuint>> mTextureAddrs;

		vector<GLfloat> mBufferData;
		vector<GLuint> mElementData;

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
			for (int i = 0; i < 36; i++)
				mElementData.push_back(i);

			glGenVertexArrays(1, &mVAOAddr);
			glBindVertexArray(mVAOAddr);

			glGenBuffers(1, &mVBOAddr);
			glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
			glBufferData(GL_ARRAY_BUFFER, mBufferData.size() * sizeof(GLfloat), mBufferData.data(), GL_STATIC_DRAW); // ??

			glGenBuffers(1, &mEBOAddr);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, mElementData.size() * sizeof(GLuint), mElementData.data(), GL_STATIC_DRAW);

			GLuint stride = 10 * sizeof(GLfloat);
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
			glUniformMatrix4fv(glGetUniformLocation(prog_addr, "modelMat"),
			                   1, false, GetModelMatrix());

			// 규약: mTextureAddrs[0]     = tex1 (모든 면 공용) → unit 0
			//       mTextureAddrs[1 + f] = tex2 (면 f=0..5)    → unit 2
			//
			// sampler uniform 은 "unit 번호" 를 int 로 받음. 한 번만 세팅하면 돼.
			glUniform1i(glGetUniformLocation(prog_addr, "tex1"), 0);
			glUniform1i(glGetUniformLocation(prog_addr, "tex2"), 2);

			// tex1 : unit 0 에 한 번만 바인딩 (루프 밖)

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0].second);

			// tex2 + draw : 면마다 텍스처 교체 → 그 면의 6 인덱스만 드로우
			//   BuildCube 가 면마다 6정점을 0..35 순차로 펼쳐 넣었으니 f*6 오프셋
			//   draw call 을 루프 안으로 옮겨야 면마다 다른 텍스처가 실제로 반영됨
			for (int f = 0; f < 6; f++)
			{
				glActiveTexture(GL_TEXTURE2);
				glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f].second);
				glDrawElements(GL_TRIANGLES,
				               6,
				               GL_UNSIGNED_INT,
				               (void *)(f * 6 * sizeof(GLuint)));
			}
		}

		void AddTexture(const char *sampler_name, const char *image_path)
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
			else
			{
				cerr << "failed to load texture: " << image_path << endl;
			}
			stbi_image_free(data);

			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			mTextureAddrs.push_back({sampler_name, texture});
		}
	};

	class MyApplication : public sb7::application
	{
		unique_ptr<ProgramBase> program;
		vector<unique_ptr<ModelBase>> models;

		vec3 eye = vec3(0.0, 1.0, 3.0);
		vec3 target = vec3(0.0, 0.0, 0.0);
		vec3 worldup = vec3(0.0, 1.0, 0.0);

		float fov = 50;
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

			model->AddTexture("tex1", "./textures/side1.jpg");
			model->AddTexture("tex2", "./textures/side1.jpg");
			model->AddTexture("tex2", "./textures/side2.jpg");
			model->AddTexture("tex2", "./textures/side3.jpg");
			model->AddTexture("tex2", "./textures/side4.jpg");
			model->AddTexture("tex2", "./textures/side5.jpg");
			model->AddTexture("tex2", "./textures/side6.jpg");

			models.push_back(std::move(model));
		};

		virtual void render(double currentTime) override
		{
			glClearBufferfv(GL_COLOR, 0, BG_COLOR);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);

			aspect = ((float)info.windowWidth) / info.windowHeight;

			glUseProgram(program->GetProgramAddr());
			glUniformMatrix4fv(
			    glGetUniformLocation(program->GetProgramAddr(), "viewMat"),
			    1, false, vmath::lookat(eye, target, worldup));
			glUniformMatrix4fv(
			    glGetUniformLocation(program->GetProgramAddr(), "projMat"),
			    1, false, vmath::perspective(fov, aspect, nearplane, farplane));

			for (auto &model : models)
			{
				model->Draw(program->GetProgramAddr());
			}

			// glDrawArrays(GL_TRIANGLES, 0, 12);
		}

		virtual void shutdown() override
		{
		}
	};
}; // namespace exercise6

DECLARE_MAIN(exercise6::MyApplication);