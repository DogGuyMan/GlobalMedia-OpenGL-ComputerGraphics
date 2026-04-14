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
	static const char *UNIFORM_UV_OFFSET = "uvOffset";
	static const char *UNIFORM_UV_RATIO = "uvRatio";

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

	static const std::vector<vmath::vec2> BASE_TRIANGLE_MESH_UVS{
	    {0.0, 0.0},
	    {1.0, 0.0},
	    {0.5, 1.0},
	};

	static const std::vector<vmath::vec2> BASE_QUAD_MESH_UVS{
	    {0.0, 0.0},
	    {1.0, 0.0},
	    {1.0, 1.0},
	    {0.0, 1.0}};

	const std::vector<GLuint> TRIANGLE_FACE_INDICES = {{0, 1, 2}};
	const std::vector<GLuint> QUAD_FACE_INDICES = {{0, 1, 2, 0, 2, 3}};

	static const vector<vmath::vec4> CONE_SIDE_BASE_POSITION = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 1.0, 1.0},
	    {0.0, 0.0, 1.0, 1.0},
	    {0.5, 1.0, 0.5, 1.0}};

	static const vector<vmath::vec4> CONE_BOTTOM_BASE_POSITION = {
	    CONE_SIDE_BASE_POSITION[0],
	    CONE_SIDE_BASE_POSITION[1],
	    CONE_SIDE_BASE_POSITION[2],
	    CONE_SIDE_BASE_POSITION[3],
	};

	static const std::vector<std::vector<GLuint>> CONE_SIDE_FACE_INDICES = {
	    {1, 0, 4},
	    {2, 1, 4},
	    {3, 2, 4},
	    {0, 3, 4},
	};

	static const vector<vmath::vec4> CONE_SIDE_BASE_COLORS{
	    vmath::vec4(1.0, 0.0, 0.0, 1.0),
	    vmath::vec4(0.0, 1.0, 1.0, 1.0),
	    vmath::vec4(0.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 0.0, 1.0, 1.0),
	};

	static const vector<vmath::vec4> CUBE_BASE_POSITIONS = {
	    {0.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 0.0, 1.0},
	    {1.0, 0.0, 1.0, 1.0},
	    {0.0, 0.0, 1.0, 1.0},
	    {0.0, 1.0, 0.0, 1.0},
	    {1.0, 1.0, 0.0, 1.0},
	    {1.0, 1.0, 1.0, 1.0},
	    {0.0, 1.0, 1.0, 1.0},
	};

	static const std::vector<std::vector<GLuint>> CUBE_FACE_INDICES = {
	    {1, 0, 4, 1, 4, 5}, // -Z
	    {2, 1, 5, 2, 5, 6}, // +X
	    {3, 2, 6, 3, 6, 7}, // +Z
	    {0, 3, 7, 0, 7, 4}, // -X
	    {0, 1, 2, 0, 2, 3}, // -Y
	    {7, 6, 5, 7, 5, 4}, // +Y
	};

	static const vector<vmath::vec4> CUBE_BASE_COLORS{
	    vmath::vec4(1.0, 0.0, 0.0, 1.0),
	    vmath::vec4(0.0, 1.0, 0.0, 1.0),
	    vmath::vec4(0.0, 0.0, 1.0, 1.0),
	    vmath::vec4(0.0, 1.0, 1.0, 1.0),
	    vmath::vec4(1.0, 0.0, 1.0, 1.0),
	    vmath::vec4(1.0, 1.0, 0.0, 1.0)};

	void PushVertex(vector<GLfloat> &vertices,
	                const vmath::vec4 pos,
	                const vmath::vec4 color,
	                const vmath::vec2 uv,
	                const vmath::vec3 &offset)
	{
		vertices.push_back(pos[0] + offset[0]);
		vertices.push_back(pos[1] + offset[1]);
		vertices.push_back(pos[2] + offset[2]);
		vertices.push_back(pos[3]);
		vertices.push_back(color[0]);
		vertices.push_back(color[1]);
		vertices.push_back(color[2]);
		vertices.push_back(color[3]);
		vertices.push_back(uv[0]);
		vertices.push_back(uv[1]);
	}

	void BuildTriangle(
	    vector<GLfloat> &buffer_data,
	    const vector<vmath::vec4> &positions,
	    const vector<vmath::vec4> &colors,
	    const vector<vmath::vec2> &uvs,
	    const vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		for (int i = 0; i < 3; i++)
			PushVertex(buffer_data,
			           positions[position_idxs[i]],
			           colors[i],
			           uvs[TRIANGLE_FACE_INDICES[i]],
			           offset);
	}

	void BuildQuad(
	    vector<GLfloat> &buffer_data,
	    const vector<vmath::vec4> &positions,
	    const vector<vmath::vec4> &colors,
	    const vector<vmath::vec2> &uvs,
	    const vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		for (int i = 0; i < 6; i++)
			PushVertex(buffer_data,
			           positions[position_idxs[i]],
			           colors[i],
			           uvs[QUAD_FACE_INDICES[i]],
			           offset);
	}

	void BuildCube(
	    vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		for (int f = 0; f < 6; f++)
			BuildQuad(buffer_data,
			          CUBE_BASE_POSITIONS,
			          CUBE_BASE_COLORS,
			          BASE_QUAD_MESH_UVS,
			          CUBE_FACE_INDICES[f],
			          offset);
	}

	void BuildCone(
	    vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data,
			              CONE_SIDE_BASE_POSITION,
			              CONE_SIDE_BASE_COLORS,
			              BASE_TRIANGLE_MESH_UVS,
			              CONE_SIDE_FACE_INDICES[f],
			              offset);
		BuildQuad(buffer_data,
		          CONE_BOTTOM_BASE_POSITION,
		          CONE_SIDE_BASE_COLORS,
		          BASE_QUAD_MESH_UVS,
		          QUAD_FACE_INDICES,
		          offset);
	}

	void BuildDisk(vector<GLfloat> &buffer_data,
	               double us, double ue, int uRes, // 각도 (0 ~ 2*PI)
	               double vs, double ve, int vRes, // 반지름 비율 (0 ~ 1)
	               float radius = 1.0f,
	               const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f))
	{
		int numCols = uRes + 1; // 가로 정점 개수
		int numRows = vRes + 1; // 세로 정점 개수

		vector<vec4> uvColors = {
			{1.0, 0.0, 0.0, 1.0},
			{1.0, 1.0, 0.0, 1.0},
			{0.0, 1.0, 0.0, 1.0},
			{0.0, 1.0, 1.0, 1.0},
			// {1.0, 1.0, 1.0, 1.0},
			// {1.0, 1.0, 1.0, 1.0},
			// {1.0, 1.0, 1.0, 1.0},
			// {1.0, 1.0, 1.0, 1.0},
		};
		vector<vec4> diskPositions;
		vector<vec4> diskColors;
		vector<vec2> diskUVs;
		double deltaRad = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;
		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentRad = (vs + row * deltaRad) * radius;
				double currentAngle = (us + col * deltaAngle);

				diskPositions.push_back(vmath::vec4(
				    currentRad * cos(currentAngle),
				    0.0,
				    -currentRad * sin(currentAngle),
				    1.0f));
				diskUVs.push_back(vec2((float)col / uRes, (float)row / vRes));

				// u 축으로 2차 선형보간
				float adjU = (float)col / uRes; // u축에 더 가까움
				float adjV = (float)row / vRes; // v축에 더 가까움
				auto u1Color = (1 - adjU) * uvColors[0] + (adjU)*uvColors[1];
				auto u2Color = (1 - adjU) * uvColors[3] + (adjU)*uvColors[2];
				auto interpoatedColor = (1 - adjV) * u2Color + (adjV) * u1Color;
				diskColors.push_back(interpoatedColor);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;

				// CCW Quad indices
				int indices[] = {p0, p1, p2, p0, p2, p3};

				for (int idx : indices)
					PushVertex(buffer_data, diskPositions[idx], diskColors[idx], diskUVs[idx], offset);
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

		vec2 mUVOffset = vec2(0.0f, 0.0f);
		vec2 mUVRatio = vec2(1.0f, 1.0f);

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
			// vec2 uniform 은 glUniform2fv — Matrix4fv 는 mat4 (16 float) 용
			glUniform2fv(glGetUniformLocation(prog_addr, UNIFORM_UV_OFFSET), 1, mUVOffset);
			glUniform2fv(glGetUniformLocation(prog_addr, UNIFORM_UV_RATIO), 1, mUVRatio);

			glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX1), 0);
			// glUniform1i(glGetUniformLocation(prog_addr, SAMPLER_TEX2), 1);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, mTextureAddrs[0]);

			// for (int f = 0; f < 6; f++)
			// {
			// 	glActiveTexture(GL_TEXTURE1);
			// 	glBindTexture(GL_TEXTURE_2D, mTextureAddrs[1 + f]);
			// 	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void *)(f * 6 * sizeof(GLuint)));
			// }

			glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
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
			{
				// BuildCube(vertices);
				// auto model = std::make_unique<ModelBase>();
				// model->Build(vertices);

				// model->AddTexture(TEXTURE_CONTAINER);
				// for (int f = 0; f < 6; f++)
				// 	model->AddTexture(TEXTURE_SIDES[f]);

				// model->mScale = vec3(0.75, 0.75, 0.75);
				// models.push_back(std::move(model));
			}
			{
				// BuildCone(vertices);

				// auto model = std::make_unique<ModelBase>();
				// model->Build(vertices);

				// model->AddTexture(TEXTURE_CONTAINER);
				// for (int f = 0; f < 4; f++)
				// 	model->AddTexture(TEXTURE_SIDES[0]);
				// model->mScale = vec3(0.75, 0.75, 0.75);
				// models.push_back(std::move(model));
			}

			{
				double disRad = 1.0f;
				BuildDisk(vertices,
				          0, 2 * M_PI, 32,
				          0.5, 1.0, 1, disRad);
				for (int j = 0; j < vertices.size() / 10; j++)
				{
					for (int i = 0; i < 10; i++)
					{
						cout << vertices[j * 10 + i] << " ";
					}
					cout << endl;
				}
				auto model = std::make_unique<ModelBase>();
				model->Build(vertices);
				model->AddTexture(TEXTURE_CONTAINER);
				model->mScale = vec3(1.0, 1.0, 1.0);
				models.push_back(std::move(model));
			}
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
			// models.back()->mEulerRot = vmath::vec3(angle, angle, angle);
			models.back()->mUVOffset = vmath::vec2(currentTime, 1.0f);

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