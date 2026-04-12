#include <GL/gl3w.h>
#include <GL/glcorearb.h>
#include <cstdlib>
#include <sb7.h>
#include <shader.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <vmath.h>

#include <iostream>
#include <memory>
#include <utility>
#include <vector>

static const vmath::vec4 BASE_COLORS[6]{
    vmath::vec4(1.0, 0.0, 0.0, 0.0),
    vmath::vec4(0.0, 1.0, 0.0, 0.0),
    vmath::vec4(0.0, 0.0, 1.0, 0.0),
    vmath::vec4(0.0, 1.0, 1.0, 0.0),
    vmath::vec4(1.0, 0.0, 1.0, 0.0),
    vmath::vec4(1.0, 1.0, 0.0, 0.0)};

/*********************************************************************************
 *
 * HEADER
 *
 *********************************************************************************/

namespace Chapter7::Model
{

class ModelBase
{
  protected:
	GLuint mVAOAddr;
	GLuint mVBOAddr;
	GLuint mEBOAddr;
	GLuint mTexAddr;

	vmath::vec4 mPivot;

	vmath::vec4 mTranslateVec;
	vmath::vec4 mEulerRotateVec;
	vmath::vec4 mScaleVec;

	std::vector<GLfloat> mBufferObject;
	std::vector<GLuint> mElementBuffer;

	bool mIsBuilted = false;

  public:
	ModelBase(vmath::vec3 _pivot = vmath::vec3(0.0, 0.0, 0.0));
	virtual ~ModelBase();
	vmath::vec3 GetTranslate() const;
	void SetTranslate(vmath::vec3 vec);
	vmath::vec3 GetEulerRotate() const;
	void SetEulerRotate(vmath::vec3 vec);
	vmath::vec3 GetScale() const;
	void SetScale(vmath::vec3 vec);
	vmath::vec3 GetPivot() const;
	void SetPivot(vmath::vec3 vec);
	vmath::mat4 GetModelMatrix();
	GLuint GetVertexArrayObject() const;

	virtual ModelBase &Build() = 0;
	virtual void Deconstruct() = 0;
	// ! 폐기 : SetTexture(int width, int height, int channels, const char* image_path)
	//          width/height/channels는 stbi_load의 *출력* 인자라 호출자가 넘긴 값이 즉시 덮어써짐 → 무의미
	virtual ModelBase &SetTexture(const char *image_path) = 0;
	virtual void Draw(GLuint program_address) = 0;
};

class PlaneModel : public ModelBase
{
  protected:
	// [z][y][x]
	const vmath::vec4 mPlaneVertices[2][2] = {
	    {
	        {vmath::vec4(0.0, 0.0, 0.0, 1.0)},
	        {vmath::vec4(1.0, 0.0, 0.0, 1.0)},
	    },
	    {
	        {vmath::vec4(0.0, 1.0, 0.0, 1.0)},
	        {vmath::vec4(1.0, 1.0, 0.0, 1.0)},
	    },
	};
	void pushVertex(int element_idx, std::pair<int, int> pos, std::pair<double, double> uvcoord);
	void initModelData();

  public:
	PlaneModel(vmath::vec3 _pivot = vmath::vec3(0.5, 0.5, 0.0));
	~PlaneModel();
	virtual ModelBase &Build() override;
	virtual void Deconstruct() override;
	virtual ModelBase &SetTexture(const char *image_path) override;
	virtual void Draw(GLuint program_address) override;
};

} // namespace Chapter7::Model
namespace Chapter7::Program
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
	~ProgramBase();
	// ! 폐기 : void PushModel(std::unique_ptr<Model::ModelBase>&& model);
	// unique_ptr는 move-only 타입이라 by value로 받는 게 idiomatic. 호출 코드는 그대로 std::move(plane) 사용
	void PushModel(std::unique_ptr<Model::ModelBase> model);
	const std::vector<std::unique_ptr<Model::ModelBase>> &GetModels() const;
	void UseProgram();
	GLuint GetProgramAddress() const;
};
}; // namespace Chapter7::Program

namespace Chapter7::Camera
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
}; // namespace Chapter7::Camera

namespace Chapter7::Resources
{
class ResourceManager
{
};
} // namespace Chapter7::Resources

/*********************************************************************************
 *
 * SOURCE
 *
 *********************************************************************************/
namespace Chapter7::Model
{

ModelBase::ModelBase(vmath::vec3 _pivot)
    : mPivot(vmath::vec4(_pivot[0], _pivot[1], _pivot[2], 0.0)), // ! A2 :mTranslateVec/mEulerRotateVec/mScaleVec/mPivot 초기화 안 됨 -> 가비지 값으로 모델 행렬 계산
      mTranslateVec(vmath::vec4(0.0, 0.0, 0.0, 0.0)),            // ! A2 :mTranslateVec/mEulerRotateVec/mScaleVec/mPivot 초기화 안 됨 -> 가비지 값으로 모델 행렬 계산
      mEulerRotateVec(vmath::vec4(0.0, 0.0, 0.0, 0.0)),          // ! A2 :mTranslateVec/mEulerRotateVec/mScaleVec/mPivot 초기화 안 됨 -> 가비지 값으로 모델 행렬 계산
      mScaleVec(vmath::vec4(1.0, 1.0, 1.0, 1.0))                 // ! A2 :mTranslateVec/mEulerRotateVec/mScaleVec/mPivot 초기화 안 됨 -> 가비지 값으로 모델 행렬 계산
                                                                 // TODO 멤버 초기화 리스트로
{
	// ! A1 build()  base 생성자 -> build() -> 순수가상 initModelData() 호출 -> crash
	// TODO laneModel 생성자(또는 정적 팩토리)에서 호출
}

ModelBase::~ModelBase()
{
}

vmath::vec3 ModelBase::GetTranslate() const
{
	return {mTranslateVec[0], mTranslateVec[1], mTranslateVec[2]};
}
void ModelBase::SetTranslate(vmath::vec3 vec)
{
	mTranslateVec = {vec[0], vec[1], vec[2], 0.0};
}
vmath::vec3 ModelBase::GetEulerRotate() const
{
	return {mEulerRotateVec[0], mEulerRotateVec[1], mEulerRotateVec[2]};
}
void ModelBase::SetEulerRotate(vmath::vec3 vec)
{
	mEulerRotateVec = {vec[0], vec[1], vec[2], 0.0};
}
vmath::vec3 ModelBase::GetScale() const
{
	return {mScaleVec[0], mScaleVec[1], mScaleVec[2]};
}
void ModelBase::SetScale(vmath::vec3 vec)
{
	mScaleVec = {vec[0], vec[1], vec[2], 0.0};
}
vmath::vec3 ModelBase::GetPivot() const
{
	return {mPivot[0], mPivot[1], mPivot[2]};
}
void ModelBase::SetPivot(vmath::vec3 vec)
{
	mPivot = {vec[0], vec[1], vec[2], 0.0};
}
// GetModelMatrix() : Local 회전/스케일 (Unity의 transform.rotation/scale 의미)
//
// 공식 :  M = T(translate) * R * S * T(-pivot)
//
// 적용 순서 (정점 v에 M을 곱하면 오른쪽부터 적용):
//   1) T(-pivot) * v  : pivot이 원점에 오도록 평행이동
//                       (예: pivot=(0.5,0.5,0)이면 정점 [0..1]이 [-0.5..0.5]가 되어 중심이 원점)
//   2) S * v          : 원점(=local center) 기준 스케일 ← LOCAL
//   3) R * v          : 원점(=local center) 기준 회전 ← LOCAL
//   4) T(translate) * v : 월드 위치로 이동
vmath::mat4 ModelBase::GetModelMatrix()
{
	// ! C4 : 행렬 곱 순서 (T*R*S*T(-pivot))
	vmath::mat4 transMat = vmath::translate<float>(
		mTranslateVec[0], mTranslateVec[1], mTranslateVec[2]);

	vmath::mat4 xRotMat = vmath::rotate<float>(mEulerRotateVec[0], 1.0, 0.0, 0.0);
	vmath::mat4 yRotMat = vmath::rotate<float>(mEulerRotateVec[1], 0.0, 1.0, 0.0);
	vmath::mat4 zRotMat = vmath::rotate<float>(mEulerRotateVec[2], 0.0, 0.0, 1.0);

	vmath::mat4 scaleMat = vmath::scale<float>(
		mScaleVec[0], mScaleVec[1], mScaleVec[2]);

	// pivot 보정 : local space의 mPivot 점을 원점으로 끌어오는 평행이동 (negate 주의)
	vmath::mat4 pivotMat = vmath::translate<float>(
		-mPivot[0], -mPivot[1], -mPivot[2]);

	// ! 폐기 : return transMat * zRotMat * yRotMat * xRotMat * scaleMat * modelMatrix;
	//          (mOffset을 transMat에 합쳐 넣어서 pivot 역할을 못 함 — 모서리 기준 회전 발생)
	return transMat * zRotMat * yRotMat * xRotMat * scaleMat * pivotMat;
}

GLuint ModelBase::GetVertexArrayObject() const
{
	return mVAOAddr;
}

/*********************************************************************************
 *
 * Plane
 *
 *********************************************************************************/

void PlaneModel::pushVertex(int element_idx, std::pair<int, int> pos, std::pair<double, double> uvcoord)
{
	for (int i = 0; i < 4; i++)
		mBufferObject.push_back(mPlaneVertices[pos.second][pos.first][i]);
	for (int i = 0; i < 4; i++)
		mBufferObject.push_back(BASE_COLORS[element_idx][i]);
	mBufferObject.push_back(uvcoord.first);
	mBufferObject.push_back(uvcoord.second);
}

// initModelData() : CPU only — GL 호출 0개
// "이 도형이 어떤 정점/인덱스로 이루어지는가"만 정의 (도형마다 다름)
// pushVertex는 mBufferObject(std::vector)에, mElementBuffer는 std::vector에 직접 대입
// -> GL 상태 의존이 없으므로 Build() 어디서 호출되어도 무방
void PlaneModel::initModelData()
{
	pushVertex(0, {0, 0}, {0.0, 0.0}); // A
	pushVertex(1, {1, 0}, {1.0, 0.0}); // B
	pushVertex(2, {1, 1}, {1.0, 1.0}); // C
	pushVertex(3, {0, 1}, {0.0, 1.0}); // D

	mElementBuffer = {0, 1, 2, 0, 2, 3};
}

PlaneModel::PlaneModel(vmath::vec3 _pivot)
    : ModelBase(_pivot)
{
}

PlaneModel::~PlaneModel()
{
	Deconstruct();
}

// Build() : GPU 업로드 boilerplate
// 책임 분리 — initModelData()(CPU 데이터 정의)와 GPU 리소스 생성/바인딩/업로드는 독립적
// 모든 모델의 Build()가 거의 동일한 패턴이므로 향후 ModelBase로 끌어올릴 여지가 있음
//
// !!! 절대 어기면 안 되는 순서 !!!
//	glBindVertexArray(VAO)
//		glBindBuffer(GL_ARRAY_BUFFER, VBO)
//			glBufferData(...)               ← VBO 바인딩 후 + CPU 데이터 준비 후
//			glVertexAttribPointer(...)      ← VBO 바인딩 후 + VAO 바인딩 중 (VAO에 기록)
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO)  ← VAO 바인딩 중이어야 EBO가 VAO에 기록됨
//		glBufferData(GL_ELEMENT_ARRAY_BUFFER, ...)
ModelBase &PlaneModel::Build()
{
	if (mIsBuilted)
		return *this;

	// 1) CPU 데이터 준비 (GL 호출 0개)
	initModelData();

	// 2) VAO 생성/바인딩 — 이후 attribute pointer / EBO 바인딩이 VAO에 기록됨
	glGenVertexArrays(1, &mVAOAddr);
	// ! glBindVertexArray(mVAOAddr);
	glBindVertexArray(mVAOAddr); // TODO glBindVertexArray(mVAOAddr); 이게 맞다

	// 3) VBO 생성 -> 바인딩 -> 업로드 (mBufferObject가 채워진 후여야 함)
	glGenBuffers(1, &mVBOAddr);
	glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
	glBufferData(GL_ARRAY_BUFFER, (mBufferObject.size() * sizeof(GLfloat)), mBufferObject.data(), GL_STATIC_DRAW);

	// 4) EBO 생성 -> 바인딩 -> 업로드. VAO 바인딩 중이라 EBO 바인딩이 VAO에 기록됨
	glGenBuffers(1, &mEBOAddr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
	// ! A4 : glBufferData(GL_ELEMENT_ARRAY_BUFFER, (mElementBuffer.size() * sizeof(GLfloat)), mElementBuffer.data(), GL_STATIC_DRAW); EBO glBufferData 크기 계산이 float이 아님
	// TODO sizeof(GLuint)로 변경
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (mElementBuffer.size() * sizeof(GLuint)), mElementBuffer.data(), GL_STATIC_DRAW);

	// 5) attribute layout — VBO가 GL_ARRAY_BUFFER에 바인딩된 상태에서 호출. VAO에 기록됨
	GLuint stride = 10 * sizeof(GLfloat);
	void *vertex_index = (void *)(sizeof(float) * 0);
	void *color_index = (void *)(sizeof(float) * 4);
	void *uv_index = (void *)(sizeof(float) * 8);

	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, vertex_index); // ! 함수 이름 외우기
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, color_index); // ! 함수 이름 외우기
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, uv_index); // ! 함수 이름 외우기
	glEnableVertexAttribArray(2);

	mIsBuilted = true;
	return *this;
};
void PlaneModel::Deconstruct()
{
	if (!mIsBuilted)
		return;
	glDeleteVertexArrays(1, &mVAOAddr);
	mIsBuilted = false;
}

// ! 폐기 : ModelBase& PlaneModel::SetTexture(int width, int height, int nr_chennels, const char* image_path)
//          width/height/nr_chennels는 stbi_load의 출력 인자라 호출자가 넘긴 값이 즉시 덮어써짐 → 무의미
ModelBase &PlaneModel::SetTexture(const char *image_path)
{
	glGenTextures(1, &mTexAddr);
	glBindTexture(GL_TEXTURE_2D, mTexAddr);

	// stbi_load가 width/height/channels를 *출력*으로 채워줌 → 로컬 변수로 받기
	int width, height, nr_chennels;
	auto *tex_ptr = stbi_load(image_path, &width, &height, &nr_chennels, 0);
	if (tex_ptr != nullptr)
	{
		glTexImage2D(
		    GL_TEXTURE_2D, 0, GL_RGB,
		    width, height, 0, GL_RGB,
		    GL_UNSIGNED_BYTE, tex_ptr);
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	stbi_image_free(tex_ptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return *this;
};

void PlaneModel::Draw(GLuint program_address)
{
	// 텍스처 unit 0 활성화 + 텍스처 바인딩 + sampler uniform("tex1") → unit 0
	// (이 호출들이 빠지면 SetTexture()로 만든 텍스처가 셰이더에 전달되지 않음)
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTexAddr);
	glUniform1i(glGetUniformLocation(program_address, "tex1"), 0);

	glBindVertexArray(mVAOAddr);
	glUniformMatrix4fv(glGetUniformLocation(program_address, "modelMat"), 1, false, GetModelMatrix());
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

}; // namespace Chapter7::Model

namespace Chapter7::Program
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
    : VS_PATH("./shaders/default_vs.glsl"), FS_PATH("./shaders/default_fs.glsl")
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

// ! 폐기 : void ProgramBase::PushModel(std::unique_ptr<Model::ModelBase>&& model)
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

}; // namespace Chapter7::Program

namespace Chapter7::Camera
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
	// ! C4 : return vmath::mat4::identity() * translateMat; 행렬 순서가 거꾸로 됨
	// TODO 표준 T * R * S (translate * rot * scale)
	return translateMat * vmath::mat4::identity();
}

vmath::mat4 Camera::GetViewMatrix() const
{
	// ! C1 : return GetModelMatrix() * vmath::lookat(mEye, mTarget, mWorldUp); lookat이 이미 view 행렬임. 이중 변환
	// TODO vmath::lookat 만 반환하기
	return vmath::lookat(mEye, mTarget, mWorldUp);
}

vmath::mat4 Camera::GetProjectionMatrix(int window_width, int window_height) const
{
	// ! C2 : return Camera::GetViewMatrix() * vmath::perspective(mFov, (float)window_height / window_height, mNearPlane, mFarPlane);
	// TODO vmath::perspective 만 반환하기
	return vmath::perspective(mFov, ((float)window_width / window_height), mNearPlane, mFarPlane);
}
}; // namespace Chapter7::Camera

namespace Chapter7
{

class MyApplicaion : public sb7::application
{
  private:
	std::vector<std::unique_ptr<Program::ProgramBase>> programs;
	Camera::Camera camera;

  public:
	virtual void startup() override
	{
		// stbi_set_flip_vertically_on_load(true);
		programs.push_back(std::make_unique<Program::ProgramBase>());
		camera = Camera::Camera(
		    {0.0, 0.0, 2.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
		    60, 0.1, 1000.0);

		for(int i = 0; i < 4; i++){
			// ! 폐기 : vmath::vec4{-0.5, -0.5, 0.0, 0.0} (mOffset = 음수 평행이동)
			// pivot = (0.5, 0.5, 0) — 평면의 기하학적 중심. default와 동일하므로 인자 생략 가능
			auto plane = std::make_unique<Model::PlaneModel>(vmath::vec3{0.5, 0.5, 0.0});
			plane->Build().SetTexture("./textures/wall.jpg");
			programs[0]->PushModel(std::move(plane));
		}

		auto& models = programs[0]->GetModels();
		models[0]->SetTranslate({-1.0, 0.0, 0.0});
		models[1]->SetTranslate({1.0, 0.0, 0.0});
		models[2]->SetTranslate({0.0, -1.0, 0.0});
		models[3]->SetTranslate({0.0, 1.0, 0.0});
	}
	virtual void render(double currentTime) override
	{
		// TODO D6 : startup에서 활성화 (필요 시 glEnable(GL_CULL_FACE)도)
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		const GLfloat backgroundColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
		glClearBufferfv(GL_COLOR, 0, backgroundColor); // ?

		float angle = vmath::radians((currentTime * 180) /3.14) * 10;

		for (const auto &prog : programs)
		{
			prog->UseProgram();
			GLuint progAddr = prog->GetProgramAddress();
			// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "modelMat"), 1, false, model->GetModelMatrix()); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "viewMat"), 1, false, camera.GetViewMatrix()); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "projMat"), 1, false, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight)); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
			// TODO 모델은 단일 루프로 통합, view/proj는 Draw 호출 *전*에 set해야 첫 프레임부터 정상
			glUniformMatrix4fv(glGetUniformLocation(progAddr, "viewMat"), 1, false, camera.GetViewMatrix());
			glUniformMatrix4fv(glGetUniformLocation(progAddr, "projMat"), 1, false, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

			auto& models = prog->GetModels();
			for (int i = 0; i < models.size(); i++) {
				if(i % 2 == 0) models[i]->SetEulerRotate({angle, 0, angle});
				else models[i]->SetEulerRotate({0, angle*2, angle*4});
			}

			for (const auto &model : prog->GetModels())
				model->Draw(progAddr);
		}
	}

	virtual void shutdown() override
	{
	}
};
}; // namespace Chapter7

DECLARE_MAIN(Chapter7::MyApplicaion);