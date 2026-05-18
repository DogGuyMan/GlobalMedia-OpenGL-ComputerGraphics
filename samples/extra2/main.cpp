#include <GL/gl3w.h>
#include <GL/glcorearb.h>
#include <ostream>
#include <sb7.h>
#include <shader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vmath.h>

#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#define PI 3.14159

namespace exercise::Program
{
	class ProgramBase;
}

/*********************************************************************************
 *
 * MESHES — 비-parametric 하드코딩 메쉬 데이터용 구조체
 *
 * parametric Surface로 표현하기 어려운 도형(또는 parametric 결과를 수작업 편집한 것)을
 * vertex/index 배열로 담아 ModelBase의 direct-mesh 생성자에 전달
 *
 * 구조체 정의는 유지하되 "완제품 Cube 함수"는 제공하지 않음 —
 * 사용자가 ModelBase::PrintMeshData() 출력을 복사/편집하여 직접 구성
 *
 *********************************************************************************/

namespace exercise::Meshes
{

	static const std::vector<vmath::vec2> BASE_MESH_UVS{
	    {0.0, 0.0},
	    {1.0, 0.0},
	    {1.0, 1.0},
	    {0.0, 1.0}};

	static const vmath::vec4 BASE_COLORS[6]{
	    vmath::vec4(1.0, 0.0, 0.0, 0.0),
	    vmath::vec4(0.0, 1.0, 0.0, 0.0),
	    vmath::vec4(0.0, 0.0, 1.0, 0.0),
	    vmath::vec4(0.0, 1.0, 1.0, 0.0),
	    vmath::vec4(1.0, 0.0, 1.0, 0.0),
	    vmath::vec4(1.0, 1.0, 0.0, 0.0)};

	namespace Triangle
	{

		static const std::vector<std::vector<vmath::vec4>> TRIANGLE_BASE_POSITIONS = {
		    std::vector<vmath::vec4>{
		        {0.0, 0.0, 0.0, 1.0},
		        {1.0, 0.0, 0.0, 1.0},
		        {1.0, 1.0, 0.0, 1.0},
		    },
		    std::vector<vmath::vec4>{
		        {0.0, 0.0, 0.0, 1.0},
		        {1.0, 0.0, 0.0, 1.0},
		        {0.5, 0.866, 0.0, 1.0},
		    }};

		static const std::vector<GLuint> TRIANGLE_BASE_INDICES = {
		    0, 1, 2};
	} // namespace Triangle

	namespace Plane
	{
		static const std::vector<std::vector<vmath::vec4>> QUAD_BASE_POSITIONS = {
		    std::vector<vmath::vec4>{
		        {0.0, 0.0, 0.0, 1.0},
		        {1.0, 0.0, 0.0, 1.0},
		        {1.0, 1.0, 0.0, 1.0},
		        {0.0, 1.0, 0.0, 1.0},
		    }};
		static const std::vector<GLuint> QUAD_BASE_INDICES = {
		    0, 1, 2, 0, 2, 3};

	} // namespace Plane

	namespace Cube
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

		static const std::vector<GLuint> QUAD_BASE_INDICES[6] = {
		    {0, 1, 5, 0, 5, 4},
		    {1, 2, 6, 1, 6, 5},
		    {2, 3, 7, 2, 7, 6},
		    {3, 0, 4, 3, 4, 7},
		    {0, 1, 2, 0, 2, 3},
		    {4, 5, 6, 4, 6, 7},
		};

	} // namespace Cube

	namespace Cone
	{

	}

	struct MeshData
	{
		std::vector<float> vertices;
		std::vector<GLuint> elements;
	};

	inline MeshData BuildTriangle(int variant = 1)
	{
		MeshData md;
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
				md.vertices.push_back(Triangle::TRIANGLE_BASE_POSITIONS[variant][i][j]);
			for (int c = 0; c < 4; c++)
				md.vertices.push_back(BASE_COLORS[i][c]);
			for (int a = 0; a < 2; a++)
				md.vertices.push_back(BASE_MESH_UVS[i][a]);
		}
		md.elements = Triangle::TRIANGLE_BASE_INDICES;
		return md;
	}

	inline MeshData BuildPlane()
	{
		MeshData md;
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
				md.vertices.push_back(Plane::QUAD_BASE_POSITIONS[0][i][j]);
			for (int c = 0; c < 4; c++)
				md.vertices.push_back(BASE_COLORS[i][c]);
			for (int a = 0; a < 2; a++)
				md.vertices.push_back(BASE_MESH_UVS[i][a]);
		}
		md.elements = Plane::QUAD_BASE_INDICES;
		return md;
	}

	inline MeshData BuildCubeFace(int f, const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		MeshData md;
		const int uvIdx[6] = {0, 1, 2, 0, 2, 3};
		const vmath::vec4 *cubeVertices = &Cube::CUBE_BASE_POSITIONS[0][0];

		const auto &faceIdx = Cube::QUAD_BASE_INDICES[f];
		const auto &color = BASE_COLORS[f];
		for (int i = 0; i < 6; i++)
		{
			const auto &pos = cubeVertices[faceIdx[i]];
			const auto &uv = BASE_MESH_UVS[uvIdx[i]];
			md.vertices.push_back(pos[0] + offset[0]);
			md.vertices.push_back(pos[1] + offset[1]);
			md.vertices.push_back(pos[2] + offset[2]);
			md.vertices.push_back(pos[3]);
			for (int c = 0; c < 4; c++)
				md.vertices.push_back(color[c]);
			for (int a = 0; a < 2; a++)
				md.vertices.push_back(uv[a]);
		}
		md.elements = {0, 1, 2, 3, 4, 5};
		return md;
	}

	inline MeshData BuildCube(const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		MeshData md;
		for (int f = 0; f < 6; f++)
		{
			MeshData face = BuildCubeFace(f, offset);
			const GLuint base = static_cast<GLuint>(md.vertices.size() / 10);
			for (float v : face.vertices)
				md.vertices.push_back(v);
			for (GLuint idx : face.elements)
				md.elements.push_back(base + idx);
		}
		return md;
	}

	inline MeshData BuildDisk(const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f))
	{
		MeshData md;
		return md;
	}
} // namespace exercise::Meshes

/*********************************************************************************
 *
 * HEADER
 *
 *********************************************************************************/

namespace exercise::Model
{

	class Transform
	{
	  private:
		vmath::vec4 mPivot;
		vmath::vec4 mTranslateVec;
		vmath::vec4 mEulerRotateVec;
		vmath::vec4 mScaleVec;

	  public:
		Transform(vmath::vec3 pivot = vmath::vec3(0.0f, 0.0f, 0.0f));

		vmath::vec3 GetTranslate() const;
		Transform &SetTranslate(vmath::vec3 vec);
		vmath::vec3 GetEulerRotate() const;
		Transform &SetEulerRotate(vmath::vec3 vec);
		vmath::vec3 GetScale() const;
		Transform &SetScale(vmath::vec3 vec);
		vmath::vec3 GetPivot() const;
		Transform &SetPivot(vmath::vec3 vec);

		vmath::mat4 GetModelMatrix() const;
	};

	//

	class Material
	{
	  public:
		struct TextureSlot
		{
			GLuint addr;
			GLenum target;
			std::string samplerName;
			int unit;
		};

	  private:
		vmath::vec4 mBaseColor;
		vmath::vec2 mUVOffset;
		vmath::vec2 mUVRatio;

	  public:
		std::vector<TextureSlot> mTextures;
		Material(vmath::vec4 baseColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		~Material();

		Material(const Material &) = delete;
		Material &operator=(const Material &) = delete;
		Material(Material &&) = default;
		Material &operator=(Material &&) = default;

		vmath::vec4 GetBaseColor() const;
		Material &SetBaseColor(vmath::vec4 color);
		vmath::vec2 GetUVOffset() const;
		Material &SetUVOffset(vmath::vec2 vec);
		vmath::vec2 GetUVRatio() const;
		Material &SetUVRatio(vmath::vec2 vec);

		// samplerName : 셰이더 uniform sampler 이름 ("tex1", "tex2" …)
		// unit        : 이 텍스처가 바인딩될 texture unit 번호 (0, 1, 2 …)
		//               -> Apply() 에서 glActiveTexture(GL_TEXTURE0 + unit) + glUniform1i(sampler, unit)
		Material &AddTexture2D(const std::string &samplerName, const char *image_path, int unit = 0);

		// glDrawElements 직전 호출 — 모든 슬롯을 각자의 unit 에 바인딩 + sampler uniform 설정
		void Apply(Program::ProgramBase &prog);
	};

	class ModelBase
	{
	  protected:
		GLuint mVAOAddr;
		GLuint mVBOAddr;
		GLuint mEBOAddr;

		Transform mTransform;
		Material mMaterial;

		std::vector<GLfloat> mBufferObject;
		std::vector<GLuint> mElementBuffer;

		bool mIsBuilted = false;

		bool mUseDirectMesh = false;
		GLsizei mIndexCount = 0;

		void initModelData();

	  public:
		ModelBase(std::vector<GLfloat> vertices, std::vector<GLuint> indices,
		          vmath::vec3 _pivot = vmath::vec3(0.0, 0.0, 0.0));
		virtual ~ModelBase();

		Transform &GetTransform();
		const Transform &GetTransform() const;
		Material &GetMaterial();
		const Material &GetMaterial() const;

		GLuint GetVertexArrayObject() const;

		ModelBase &Build();
		void Deconstruct();
		void Draw(Program::ProgramBase &prog);
	};

} // namespace exercise::Model
namespace exercise::Program
{

	class ProgramBase
	{
	  private:
		const char *VS_PATH;
		const char *FS_PATH;

	  protected:
		GLuint mProgramAddr;
		std::vector<std::unique_ptr<Model::ModelBase>> mModels;

		GLuint createShader(GLenum shader_type, const char *shader_path);

	  public:
		ProgramBase();

		ProgramBase(const char *vs_path, const char *fs_path);
		~ProgramBase();
		void PushModel(std::unique_ptr<Model::ModelBase> model);
		const std::vector<std::unique_ptr<Model::ModelBase>> &GetModels() const;
		void UseProgram();
		GLuint GetProgramAddress() const;
	};
}; // namespace exercise::Program

namespace exercise::Camera
{
	class Camera
	{
	  private:
		vmath::vec3 mEye;
		vmath::vec3 mTarget;
		vmath::vec3 mWorldUp;

		float mFov;
		float mNearPlane;
		float mFarPlane;

	  public:
		Camera(
		    vmath::vec3 eye = vmath::vec3(0.0, 0.0, -1.0),
		    vmath::vec3 target = vmath::vec3(0.0, 0.0, 0.0),
		    vmath::vec3 world_up = vmath::vec3(0.0, 1.0, 0.0),
		    float fov = 60, float near_plane = 0.1, float far_plane = 1000.0);
		~Camera();
		vmath::vec3 GetPosition() const;
		void SetPosition(vmath::vec3 t);
		vmath::vec3 GetTowardVector() const;
		void SetTowardVector(vmath::vec3 forward);
		vmath::mat4 GetModelMatrix() const;
		vmath::mat4 GetViewMatrix() const;
		vmath::mat4 GetProjectionMatrix(int window_width, int window_height) const;
	};
}; // namespace exercise::Camera

/*********************************************************************************
 *
 * SOURCE
 *
 *********************************************************************************/
namespace exercise::Model
{

	Transform::Transform(vmath::vec3 pivot)
	    : mPivot(vmath::vec4(pivot[0], pivot[1], pivot[2], 0.0f)),
	      mTranslateVec(vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f)),
	      mEulerRotateVec(vmath::vec4(0.0f, 0.0f, 0.0f, 0.0f)),
	      mScaleVec(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f))
	{
	}

	vmath::vec3 Transform::GetTranslate() const
	{
		return {mTranslateVec[0], mTranslateVec[1], mTranslateVec[2]};
	}
	Transform &Transform::SetTranslate(vmath::vec3 vec)
	{
		mTranslateVec = {vec[0], vec[1], vec[2], 0.0f};
		return *this;
	}
	vmath::vec3 Transform::GetEulerRotate() const
	{
		return {mEulerRotateVec[0], mEulerRotateVec[1], mEulerRotateVec[2]};
	}
	Transform &Transform::SetEulerRotate(vmath::vec3 vec)
	{
		mEulerRotateVec = {vec[0], vec[1], vec[2], 0.0f};
		return *this;
	}
	vmath::vec3 Transform::GetScale() const
	{
		return {mScaleVec[0], mScaleVec[1], mScaleVec[2]};
	}
	Transform &Transform::SetScale(vmath::vec3 vec)
	{
		mScaleVec = {vec[0], vec[1], vec[2], 0.0f};
		return *this;
	}
	vmath::vec3 Transform::GetPivot() const
	{
		return {mPivot[0], mPivot[1], mPivot[2]};
	}
	Transform &Transform::SetPivot(vmath::vec3 vec)
	{
		mPivot = {vec[0], vec[1], vec[2], 0.0f};
		return *this;
	}

	vmath::mat4 Transform::GetModelMatrix() const
	{
		vmath::mat4 transMat = vmath::translate<float>(
		    mTranslateVec[0], mTranslateVec[1], mTranslateVec[2]);

		vmath::mat4 xRotMat = vmath::rotate<float>(mEulerRotateVec[0], 1.0f, 0.0f, 0.0f);
		vmath::mat4 yRotMat = vmath::rotate<float>(mEulerRotateVec[1], 0.0f, 1.0f, 0.0f);
		vmath::mat4 zRotMat = vmath::rotate<float>(mEulerRotateVec[2], 0.0f, 0.0f, 1.0f);

		vmath::mat4 scaleMat = vmath::scale<float>(
		    mScaleVec[0], mScaleVec[1], mScaleVec[2]);

		vmath::mat4 pivotMat = vmath::translate<float>(
		    -mPivot[0], -mPivot[1], -mPivot[2]);

		return transMat * zRotMat * yRotMat * xRotMat * scaleMat * pivotMat;
	}

	Material::Material(vmath::vec4 baseColor)
	    : mBaseColor(baseColor),
	      mUVOffset(vmath::vec2(0.0f, 0.0f)),
	      mUVRatio(vmath::vec2(1.0f, 1.0f))
	{
	}

	Material::~Material()
	{

		for (const auto &slot : mTextures)
		{
			if (slot.addr != 0)
				glDeleteTextures(1, &const_cast<TextureSlot &>(slot).addr);
		}
	}

	vmath::vec4 Material::GetBaseColor() const
	{
		return mBaseColor;
	}
	Material &Material::SetBaseColor(vmath::vec4 color)
	{
		mBaseColor = color;
		return *this;
	}
	vmath::vec2 Material::GetUVOffset() const
	{
		return mUVOffset;
	}
	Material &Material::SetUVOffset(vmath::vec2 vec)
	{
		mUVOffset = vec;
		return *this;
	}
	vmath::vec2 Material::GetUVRatio() const
	{
		return mUVRatio;
	}
	Material &Material::SetUVRatio(vmath::vec2 vec)
	{
		mUVRatio = vec;
		return *this;
	}

	Material &Material::AddTexture2D(const std::string &samplerName, const char *image_path, int unit)
	{
		TextureSlot slot;
		slot.target = GL_TEXTURE_2D;
		slot.samplerName = samplerName;
		slot.unit = unit;

		glGenTextures(1, &slot.addr);
		glActiveTexture(GL_TEXTURE0 + unit);
		glBindTexture(GL_TEXTURE_2D, slot.addr);

		int width, height, channels;
		auto *tex_ptr = stbi_load(image_path, &width, &height, &channels, 0);
		if (tex_ptr != nullptr)
		{
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
			             width, height, 0, GL_RGB,
			             GL_UNSIGNED_BYTE, tex_ptr);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
		stbi_image_free(tex_ptr);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		mTextures.push_back(slot);
		return *this;
	}

	void Material::Apply(Program::ProgramBase &prog)
	{
		GLuint progAddr = prog.GetProgramAddress();

		glUniform4fv(glGetUniformLocation(progAddr, "baseColor"), 1, mBaseColor);
		glUniform2fv(glGetUniformLocation(progAddr, "uvOffset"), 1, mUVOffset);
		glUniform2fv(glGetUniformLocation(progAddr, "uvRatio"), 1, mUVRatio);

		// 슬롯마다 (자기 unit 에 바인딩 + 해당 sampler uniform 에 unit 번호 주입)
		//   AddTexture2D("tex1", ..., 0) -> unit 0, sampler "tex1"=0
		//   AddTexture2D("tex2", ..., 1) -> unit 1, sampler "tex2"=1
		for (const auto &slot : mTextures)
		{
			glActiveTexture(GL_TEXTURE0 + slot.unit);
			glBindTexture(slot.target, slot.addr);
			glUniform1i(glGetUniformLocation(progAddr, slot.samplerName.c_str()), slot.unit);
		}
	}

	ModelBase::ModelBase(std::vector<GLfloat> vertices, std::vector<GLuint> indices, vmath::vec3 _pivot)
	    : mTransform(_pivot),
	      mMaterial(),
	      mBufferObject(vertices),
	      mElementBuffer(indices),
	      mUseDirectMesh(true)
	{
	}

	ModelBase::~ModelBase()
	{
		Deconstruct();
	}

	Transform &ModelBase::GetTransform()
	{
		return mTransform;
	}
	const Transform &ModelBase::GetTransform() const
	{
		return mTransform;
	}
	Material &ModelBase::GetMaterial()
	{
		return mMaterial;
	}
	const Material &ModelBase::GetMaterial() const
	{
		return mMaterial;
	}

	GLuint ModelBase::GetVertexArrayObject() const
	{
		return mVAOAddr;
	}

	/*********************************************************************************
	 *
	 * Data Oriented ModelBase — 형태는 mSurfaceFn으로 결정, 상속 없음
	 *
	 *********************************************************************************/

	void ModelBase::initModelData()
	{

		mBufferObject.clear();
		mElementBuffer.clear();
	}

	//	glBindVertexArray(VAO)
	//		glBindBuffer(GL_ARRAY_BUFFER, VBO)
	//			glBufferData(...)               ← VBO 바인딩 후 + CPU 데이터 준비 후
	//			glVertexAttribPointer(...)      ← VBO 바인딩 후 + VAO 바인딩 중
	//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)  ← VAO 바인딩 중이어야 VAO에 기록
	//		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...)
	ModelBase &ModelBase::Build()
	{
		if (mIsBuilted)
			return *this;

		if (!mUseDirectMesh)
			initModelData();
		mIndexCount = (GLsizei)mElementBuffer.size();

		std::cout << "mBufferObject : ";
		for (auto &e : mBufferObject)
		{
			std::cout << e << " ";
		}
		std::cout << std::endl;

		std::cout << "mElementBuffer : ";
		for (auto &e : mElementBuffer)
		{
			std::cout << e << " ";
		}
		std::cout << std::endl;

		std::cout << "mIndexCount : " << mIndexCount << std::endl;

		glGenVertexArrays(1, &mVAOAddr);
		glBindVertexArray(mVAOAddr);

		glGenBuffers(1, &mVBOAddr);
		glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
		glBufferData(GL_ARRAY_BUFFER, mBufferObject.size() * sizeof(GLfloat), mBufferObject.data(), GL_STATIC_DRAW);

		glGenBuffers(1, &mEBOAddr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mElementBuffer.size() * sizeof(GLuint), mElementBuffer.data(), GL_STATIC_DRAW);

		GLuint stride = 10 * sizeof(GLfloat);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)(0));
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(8 * sizeof(float)));
		glEnableVertexAttribArray(2);

		mIsBuilted = true;
		return *this;
	}

	void ModelBase::Deconstruct()
	{
		if (!mIsBuilted)
			return;
		glDeleteVertexArrays(1, &mVAOAddr);
		glDeleteBuffers(1, &mVBOAddr);
		glDeleteBuffers(1, &mEBOAddr);

		mIsBuilted = false;
	}

	void ModelBase::Draw(Program::ProgramBase &prog)
	{
		glBindVertexArray(mVAOAddr);

		glUniformMatrix4fv(glGetUniformLocation(prog.GetProgramAddress(), "modelMat"),
		                   1, false, mTransform.GetModelMatrix());

		// 모든 텍스처 슬롯을 각자의 unit 에 바인딩 + sampler uniform 주입
		mMaterial.Apply(prog);

		glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
	}

}; // namespace exercise::Model

namespace exercise::Program
{

	GLuint ProgramBase::createShader(GLenum shader_type, const char *shader_path)
	{
		GLuint shaderAddr = sb7::shader::load(shader_path, shader_type, true);
		if (shaderAddr == 0)
		{
			std::cerr << "shader load fail : " << shader_type << ":" << shader_path << std::endl;
			exit(1);
		}
		return shaderAddr;
	}

	ProgramBase::ProgramBase()
	    : ProgramBase("./shaders/default_vs.glsl", "./shaders/default_fs.glsl")
	{
	}

	ProgramBase::ProgramBase(const char *vs_path, const char *fs_path)
	    : VS_PATH(vs_path), FS_PATH(fs_path)
	{
		mProgramAddr = glCreateProgram();
		std::vector<GLuint> shaderAddrs;

		auto vsAddr = createShader(GL_VERTEX_SHADER, VS_PATH);
		auto fsAddr = createShader(GL_FRAGMENT_SHADER, FS_PATH);
		glAttachShader(mProgramAddr, vsAddr);
		glAttachShader(mProgramAddr, fsAddr);

		glLinkProgram(mProgramAddr);

		glDeleteShader(vsAddr);
		glDeleteShader(fsAddr);
	}

	ProgramBase::~ProgramBase()
	{
	}

	void ProgramBase::PushModel(std::unique_ptr<Model::ModelBase> model)
	{
		mModels.push_back(std::move(model));
	}

	const std::vector<std::unique_ptr<Model::ModelBase>> &ProgramBase::GetModels() const
	{
		return mModels;
	}

	void ProgramBase::UseProgram()
	{
		glUseProgram(mProgramAddr);
	}

	GLuint ProgramBase::GetProgramAddress() const
	{
		return mProgramAddr;
	}

}; // namespace exercise::Program

namespace exercise::Camera
{

	Camera::Camera(vmath::vec3 eye, vmath::vec3 target, vmath::vec3 world_up, float fov, float near_plane, float far_plane)
	    : mEye(eye), mTarget(target), mWorldUp(world_up),
	      mFov(fov), mNearPlane(near_plane), mFarPlane(far_plane)
	{
	}

	Camera::~Camera()
	{
	}

	vmath::vec3 Camera::GetPosition() const
	{
		return mEye;
	}

	void Camera::SetPosition(vmath::vec3 t)
	{
		mEye = t;
	}

	vmath::vec3 Camera::GetTowardVector() const
	{
		return mTarget;
	}

	void Camera::SetTowardVector(vmath::vec3 forward)
	{
		mTarget = forward;
	}

	vmath::mat4 Camera::GetModelMatrix() const
	{
		vmath::mat4 translateMat = vmath::translate(mEye);

		return translateMat * vmath::mat4::identity();
	}

	vmath::mat4 Camera::GetViewMatrix() const
	{

		return vmath::lookat(mEye, mTarget, mWorldUp);
	}

	vmath::mat4 Camera::GetProjectionMatrix(int window_width, int window_height) const
	{

		return vmath::perspective(mFov, ((float)window_width / window_height), mNearPlane, mFarPlane);
	}
}; // namespace exercise::Camera

namespace exercise
{

	class MyApplicaion : public sb7::application
	{
	  private:
		std::vector<std::unique_ptr<Program::ProgramBase>> programs;
		Camera::Camera camera;
		const GLfloat backgroundColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	  public:
		virtual void startup() override
		{

			programs.push_back(std::make_unique<Program::ProgramBase>());
			camera = Camera::Camera(
			    {0.0, 0.0, 2.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
			    60, 0.1, 1000.0);

			programs.push_back(std::make_unique<Program::ProgramBase>(
			    "./shaders/default_vs.glsl", "./shaders/texture_fs.glsl"));

			{
				// 면별 숫자(마스크) 텍스처 — tex1, unit 0
				const char *maskPaths[6] = {
				    "./textures/side1.jpg",
				    "./textures/side2.jpg",
				    "./textures/side3.jpg",
				    "./textures/side4.jpg",
				    "./textures/side5.jpg",
				    "./textures/side6.jpg",
				};
				// 모든 면 공유 fill 텍스처 — tex2, unit 1
				const char *fillPath = "./textures/container.jpg";

				for (int f = 0; f < 6; f++)
				{
					auto md = Meshes::BuildCubeFace(f);
					auto model = std::make_unique<Model::ModelBase>(
					    std::move(md.vertices), std::move(md.elements));
					model->Build();
					model->GetTransform().SetScale(vmath::vec3{0.5, 0.5, 0.5});
					// tex1 = 숫자 마스크 (unit 0), tex2 = container fill (unit 1)
					model->GetMaterial()
					    .AddTexture2D("tex1", maskPaths[f], 0)
					    .AddTexture2D("tex2", fillPath, 1);
					programs.back()->PushModel(std::move(model));
				}
			}
		}

		virtual void render(double currentTime) override
		{

			glClearBufferfv(GL_COLOR, 0, backgroundColor);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);

			float angle = vmath::radians((currentTime * 180) / 3.14) * 10;

			for (const auto &prog : programs)
			{
				prog->UseProgram();

				glUniformMatrix4fv(glGetUniformLocation(prog->GetProgramAddress(), "viewMat"), 1, false, camera.GetViewMatrix());
				glUniformMatrix4fv(glGetUniformLocation(prog->GetProgramAddress(), "projMat"), 1, false, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

				auto &models = prog->GetModels();

				if (!models.empty())
				{
					float degY = (float)currentTime * 2 * 30.0f;
					float degX = (float)currentTime * 2 * 15.0f;
					for (auto &m : models)
					{
						m->GetTransform().SetEulerRotate({degX, degY, 0.0f});
						m->GetTransform().SetTranslate({cosf(currentTime * 2), 0.0, 0.0});
					}
				}

				for (const auto &model : prog->GetModels())
					model->Draw(*prog);
			}
		}

		virtual void shutdown() override
		{
		}
	};
}; // namespace exercise

DECLARE_MAIN(exercise::MyApplicaion);
