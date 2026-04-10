#include <sb7.h>
#include <shader.h>

#include <iostream>
#include <memory>
#include <vector>
#include <vmath.h>

namespace chapter6
{
class ITranslatable
{
  public:
	ITranslatable() = default;
	ITranslatable(ITranslatable &cpy) = delete;
	virtual ITranslatable operator=(ITranslatable &cpy) = delete;
	virtual ~ITranslatable() = default;
	virtual vmath::vec3 GetPosition() const = 0;
	virtual void SetPosition(vmath::vec3 t) = 0;
	// 누적
	virtual void Translate(vmath::vec3 toward) = 0;
};
class IRotatable
{
  public:
	IRotatable() = default;
	IRotatable(IRotatable &cpy) = delete;
	virtual IRotatable operator=(IRotatable &cpy) = delete;
	virtual ~IRotatable() = default;
	virtual vmath::vec3 GetRotation() const = 0;
	virtual void SetRotation(vmath::vec3 euler) = 0;
	// 누적
	virtual void Rotate(float angle, vmath::vec3 axis) = 0;
};

class IScalable
{
  public:
	IScalable() = default;
	IScalable(IScalable &cpy) = delete;
	virtual IScalable operator=(IScalable &cpy) = delete;
	virtual ~IScalable() = default;
	virtual vmath::vec3 GetScale() const = 0;
	virtual void SetScale(vmath::vec3 s) = 0;
	// 누적
	virtual void Scale(vmath::vec3 adj) = 0;
};

class ITransformable : public ITranslatable, IRotatable, IScalable
{
  public:
	ITransformable() = default;
	ITransformable(ITransformable &cpy) = delete;
	virtual ITransformable operator=(ITransformable &cpy) = delete;
	virtual ~ITransformable() = default;

	virtual vmath::vec3 GetPosition() const = 0;
	virtual void SetPosition(vmath::vec3 t) = 0;
	virtual void Translate(vmath::vec3 toward) = 0;
	virtual vmath::vec3 GetRotation() const = 0;
	virtual void SetRotation(vmath::vec3 euler) = 0;
	virtual void Rotate(float angle, vmath::vec3 axis) = 0;
	virtual vmath::vec3 GetScale() const = 0;
	virtual void SetScale(vmath::vec3 s) = 0;
	virtual void Scale(vmath::vec3 adj) = 0;
};

// 반례, 하면 안되는 코드‼️❌
// 하나의 mat4에 T, R, S를 각각 부분 덮어쓰기(Set) 하는 방식은 잘못된 설계이다.
// 1) mModelTransfomerMatrix[4][...] — 인덱스 4는 mat4 범위(0~3) 밖이므로 UB
// 2) SetRotation에서 rotate 행렬을 '+=' 로 합산 — 회전 합성은 곱셈이어야 함
// 3) SetRotation이 3x3 회전부를, SetPosition이 4열 이동부를 각각 덮어쓰면
//	T·R·S 합성 순서를 보장할 수 없다.
//
// 게임엔진 표준: position, euler, scale을 독립 저장 후
// GetModelMatrix()에서 T * Ry * Rx * Rz * S 순서로 합성한다.

template <class T>
class Transfomer
{
  private:
	T *mOwner;
	vmath::vec3 mPosition;
	vmath::vec3 mEulerAngles; // (pitch X, yaw Y, roll Z) 단위: degree
	vmath::vec3 mScale;

  public:
	Transfomer(T *_owner)
		: mOwner(_owner), mPosition(0.0f, 0.0f, 0.0f),
		  mEulerAngles(0.0f, 0.0f, 0.0f), mScale(1.0f, 1.0f, 1.0f)
	{
	}
	~Transfomer()
	{
		mOwner = nullptr;
	}

	// T * Ry * Rx * Rz * S 순서로 모델 행렬 합성
	vmath::mat4 GetModelMatrix() const
	{
		vmath::mat4 transMat = vmath::translate(mPosition);
		vmath::mat4 rotX =
			vmath::rotate(mEulerAngles[0], vmath::vec3(1.0f, 0.0f, 0.0f));
		vmath::mat4 rotY =
			vmath::rotate(mEulerAngles[1], vmath::vec3(0.0f, 1.0f, 0.0f));
		vmath::mat4 rotZ =
			vmath::rotate(mEulerAngles[2], vmath::vec3(0.0f, 0.0f, 1.0f));
		vmath::mat4 scaleMat = vmath::scale(mScale);
		return transMat * rotY * rotX * rotZ * scaleMat;
	}

	vmath::vec3 GetPosition() const
	{
		return mPosition;
	}
	void SetPosition(vmath::vec3 t)
	{
		mPosition = t;
	}
	void Translate(vmath::vec3 toward)
	{
		mPosition += toward;
	}

	vmath::vec3 GetRotation() const
	{
		return mEulerAngles;
	}
	void SetRotation(vmath::vec3 euler)
	{
		mEulerAngles = euler;
	}
	void Rotate(float angle, vmath::vec3 axis)
	{
		// 축 방향에 해당하는 오일러 성분에 누적
		mEulerAngles += axis * angle;
	}

	vmath::vec3 GetScale() const
	{
		return mScale;
	}
	void SetScale(vmath::vec3 s)
	{
		mScale = s;
	}
	void Scale(vmath::vec3 adj)
	{
		mScale += adj;
	}
};

class Model : public ITransformable
{
  protected:
	GLuint mVaoAddr;
	GLuint mVboAddr;
	std::vector<GLfloat> mModelData;
	Transfomer<Model> mTransformer;

  public:
	Model()
		: mTransformer(this)
	{
		glGenVertexArrays(1, &mVaoAddr);
		glBindVertexArray(mVaoAddr);

		glGenBuffers(1, &mVboAddr);
		glBindBuffer(GL_ARRAY_BUFFER, mVboAddr);
	}

	virtual ~Model()
	{
		glDeleteVertexArrays(1, &mVaoAddr);
	}

	virtual vmath::vec3 GetPosition() const override
	{
		return mTransformer.GetPosition();
	}
	virtual void SetPosition(vmath::vec3 t) override
	{
		mTransformer.SetPosition(t);
	}
	virtual void Translate(vmath::vec3 toward) override
	{
		mTransformer.Translate(toward);
	}
	virtual vmath::vec3 GetRotation() const override
	{
		return mTransformer.GetRotation();
	}
	virtual void SetRotation(vmath::vec3 euler) override
	{
		mTransformer.SetRotation(euler);
	}
	virtual void Rotate(float angle, vmath::vec3 axis) override
	{
		mTransformer.Rotate(angle, axis);
	}
	virtual vmath::vec3 GetScale() const override
	{
		return mTransformer.GetScale();
	}
	virtual void SetScale(vmath::vec3 s) override
	{
		mTransformer.SetScale(s);
	}
	virtual void Scale(vmath::vec3 adj) override
	{
		mTransformer.Scale(adj);
	}

	vmath::mat4 GetModelMatrix() const
	{
		return mTransformer.GetModelMatrix();
	}

	virtual const std::vector<GLfloat> &GetModelData() const
	{
		return mModelData;
	}

	GLuint GetVaoAddr() const
	{
		return mVaoAddr;
	}
};

class Cube : public Model
{
  private:
	vmath::vec4 mOffset;
	// mBaseVertices[z][y][x] → (x, y, z)
	// 반례, 하면 안되는 코드‼️❌
	// [1][1][0]이 (0,0,1)로 되어 있었음 → y가 1이어야 하는데 0
	// [1][1][1]이 (0,1,1)로 되어 있었음 → x가 1이어야 하는데 0
	const vmath::vec4 mBaseVertices[2][2][2] = {
		{{
		 vmath::vec4(0.0, 0.0, 0.0, 1.0), // [0][0][0] = (0,0,0)
		 vmath::vec4(1.0, 0.0, 0.0, 1.0), // [0][0][1] = (1,0,0)
		 },
		 {
		 vmath::vec4(0.0, 1.0, 0.0, 1.0), // [0][1][0] = (0,1,0)
		 vmath::vec4(1.0, 1.0, 0.0, 1.0), // [0][1][1] = (1,1,0)
		 }},
		{{
		 vmath::vec4(0.0, 0.0, 1.0, 1.0), // [1][0][0] = (0,0,1)
		 vmath::vec4(1.0, 0.0, 1.0, 1.0), // [1][0][1] = (1,0,1)
		 },
		 {
		 vmath::vec4(0.0, 1.0, 1.0, 1.0), // [1][1][0] = (0,1,1) ← y 수정
		 vmath::vec4(1.0, 1.0, 1.0, 1.0), // [1][1][1] = (1,1,1) ← x 수정
		 }}};

	const vmath::vec4 mBaseColors[6] = {
		vmath::vec4(1.0, 0.0, 0.0, 1.0), vmath::vec4(0.0, 1.0, 0.0, 1.0),
		vmath::vec4(0.0, 0.0, 1.0, 1.0), vmath::vec4(1.0, 1.0, 0.0, 1.0),
		vmath::vec4(0.0, 1.0, 1.0, 1.0), vmath::vec4(1.0, 0.0, 1.0, 1.0)};

	// 반례, 하면 안되는 코드‼️❌
	// 컴포넌트 단위로 교차 배치하면 레이아웃이 [vx,cx,vy,cy,vz,cz,vw,cw]이 되어
	// glVertexAttribPointer가 연속 4개 float를 읽을 때 엉뚱한 값을 읽게 된다.
	// for(int i = 0; i < 4; i++) {
	//	 mModelData.push_back(mBaseVertices[...][i] + mOffset[i]);
	//	 mModelData.push_back(mBaseColors[0][i]);
	// }

	// vec4 단위로 위치 4개 float → 색상 4개 float 순서로 넣어야
	// [vx,vy,vz,vw, cx,cy,cz,cw] 인터리브 레이아웃이 된다.
	void pushVertex(const vmath::vec4 &vert, const vmath::vec4 &color)
	{
		for (int i = 0; i < 4; i++)
			mModelData.push_back(vert[i] + mOffset[i]);
		for (int i = 0; i < 4; i++)
			mModelData.push_back(color[i]);
	}

	// 각 면은 해당 평면의 정점만 사용, CCW winding (법선이 바깥을 향함)
	void initModelData()
	{
		// 앞면 (z=0, -Z 법선)
		pushVertex(mBaseVertices[0][0][0], mBaseColors[0]);
		pushVertex(mBaseVertices[0][1][0], mBaseColors[0]);
		pushVertex(mBaseVertices[0][1][1], mBaseColors[0]);
		pushVertex(mBaseVertices[0][0][0], mBaseColors[0]);
		pushVertex(mBaseVertices[0][1][1], mBaseColors[0]);
		pushVertex(mBaseVertices[0][0][1], mBaseColors[0]);

		// 뒷면 (z=1, +Z 법선)
		pushVertex(mBaseVertices[1][0][0], mBaseColors[1]);
		pushVertex(mBaseVertices[1][0][1], mBaseColors[1]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[1]);
		pushVertex(mBaseVertices[1][0][0], mBaseColors[1]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[1]);
		pushVertex(mBaseVertices[1][1][0], mBaseColors[1]);

		// 윗면 (y=1, +Y 법선)
		pushVertex(mBaseVertices[0][1][0], mBaseColors[2]);
		pushVertex(mBaseVertices[1][1][0], mBaseColors[2]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[2]);
		pushVertex(mBaseVertices[0][1][0], mBaseColors[2]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[2]);
		pushVertex(mBaseVertices[0][1][1], mBaseColors[2]);

		// 아랫면 (y=0, -Y 법선)
		pushVertex(mBaseVertices[0][0][0], mBaseColors[3]);
		pushVertex(mBaseVertices[0][0][1], mBaseColors[3]);
		pushVertex(mBaseVertices[1][0][1], mBaseColors[3]);
		pushVertex(mBaseVertices[0][0][0], mBaseColors[3]);
		pushVertex(mBaseVertices[1][0][1], mBaseColors[3]);
		pushVertex(mBaseVertices[1][0][0], mBaseColors[3]);

		// 오른면 (x=1, +X 법선)
		pushVertex(mBaseVertices[0][0][1], mBaseColors[4]);
		pushVertex(mBaseVertices[0][1][1], mBaseColors[4]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[4]);
		pushVertex(mBaseVertices[0][0][1], mBaseColors[4]);
		pushVertex(mBaseVertices[1][1][1], mBaseColors[4]);
		pushVertex(mBaseVertices[1][0][1], mBaseColors[4]);

		// 왼면 (x=0, -X 법선)
		pushVertex(mBaseVertices[0][0][0], mBaseColors[5]);
		pushVertex(mBaseVertices[1][0][0], mBaseColors[5]);
		pushVertex(mBaseVertices[1][1][0], mBaseColors[5]);
		pushVertex(mBaseVertices[0][0][0], mBaseColors[5]);
		pushVertex(mBaseVertices[1][1][0], mBaseColors[5]);
		pushVertex(mBaseVertices[0][1][0], mBaseColors[5]);
	}

  public:
	Cube(vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
		: mOffset(_offset)
	{
		initModelData();

		// 반례, 하면 안되는 코드‼️❌
		// sizeof(mModelData.size()) 는 sizeof(size_t) = 8바이트를 반환한다.
		// 실제 데이터 크기가 아니라 size_t 타입의 바이트 수이므로 GPU에 8바이트만
		// 업로드된다. glBufferData(GL_ARRAY_BUFFER,
		//	 sizeof(mModelData.size()),  // ❌ 8바이트
		//	 mModelData.data(),
		//	 GL_STATIC_DRAW);

		glBufferData(GL_ARRAY_BUFFER, mModelData.size() * sizeof(GLfloat),
				 mModelData.data(), GL_STATIC_DRAW);

		// 인터리브 레이아웃: [vec4 위치, vec4 색상] = 8 floats per vertex
		GLuint stride = 8 * sizeof(float);

		// 반례, 하면 안되는 코드‼️❌
		// sizeof(float) = 4바이트는 float 1개분의 오프셋이다.
		// vec4(4개 float = 16바이트)를 건너뛰어야 색상 시작점에 도달한다.
		// const void * colorPointerOffset = (void *)sizeof(float);  // ❌ 4바이트

		// attribute 0: 위치 (offset 0)
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
		glEnableVertexAttribArray(0);
		// attribute 1: 색상 (offset 16 = vec4 하나 크기)
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
			(void *)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);
	};

	virtual ~Cube()
	{
	}
};

class Camera : public ITransformable
{
  private:
	vmath::mat4 mModelTransfomerMatrix;

	vmath::vec3 mEye;
	vmath::vec3 mTarget;
	vmath::vec3 mWorldUp;

	bool mIsPerspective = true;
	float fov;
	float nearPlane;
	float farPlane;

  public:
	// 반례, 하면 안되는 코드‼️❌
	// mWorldUp이 (0, 0.1, 0)이면 정규화 시 동일하지만 의도가 불명확하고,
	// mEye가 (5,5,5)이면 원점 기준 원운동이 너무 멀리 보인다.
	// Camera() :
	//	mEye(5.0, 5.0, 5.0), mTarget(0.0, 0.0, 0.0), mWorldUp(0.0, 0.1, 0.0),
	//	fov(60), near(0.1), far(1000.0)

	Camera()
		: mEye(0.0, 1.0, 3.0), mTarget(0.0, 0.0, 0.0), mWorldUp(0.0, 1.0, 0.0),
		fov(50), nearPlane(0.1), farPlane(1000.0)
	{
	}

	bool CheckIsPerspective()
	{
		return mIsPerspective;
	}
	Camera &SetPerspective()
	{
		mIsPerspective = true;
		return *this;
	}
	Camera &SetOthogonal()
	{
		mIsPerspective = false;
		return *this;
	}

	vmath::mat4 GetLookAtMatrix() const
	{
		return vmath::lookat(mEye, mTarget, mWorldUp);
	}

	float GetAspectRatio(int window_width, int window_height) const
	{
		return (float)window_width / window_height;
	}

	vmath::mat4 GetProjectionMatrix(int window_width, int window_height) const
	{
		return mIsPerspective
			   ? vmath::perspective(fov,
						GetAspectRatio(window_width, window_height),
						nearPlane, farPlane)
			   : vmath::ortho(-window_width / 2, window_width / 2,
					  -window_height / 2, window_height / 2, nearPlane, farPlane);
	}

	virtual vmath::vec3 GetPosition() const override
	{
		return mEye;
	}
	virtual void SetPosition(vmath::vec3 t) override
	{
		mEye = t;
	}
	virtual void Translate(vmath::vec3 toward) override
	{
		mEye += toward;
	}
	virtual vmath::vec3 GetRotation() const override
	{
		return vmath::vec3(0, 0, 0);
	}
	virtual void SetRotation(vmath::vec3 euler) override
	{
	}
	virtual void Rotate(float angle, vmath::vec3 axis) override
	{
	}
	virtual vmath::vec3 GetScale() const override
	{
		return vmath::vec3(0, 0, 0);
	}
	virtual void SetScale(vmath::vec3 s) override
	{
	}
	virtual void Scale(vmath::vec3 adj) override
	{
	}
};

class my_application : public sb7::application
{
  private:
	GLuint programAddr = 0;

	// 반례, 하면 안되는 코드‼️❌
	// application에 별도 vaoAddr를 두면 Model이 가진 VAO와 혼동된다.
	// 각 Model이 자신의 mVaoAddr를 관리하므로 여기서 따로 보관할 필요 없다.
	// GLuint vaoAddr = 0;

	const char *vs_path = "./shaders/cube_vs.glsl";
	const char *fs_path = "./shaders/cube_fs.glsl";

	std::vector<std::unique_ptr<Model>> models;
	Camera camera;

	GLuint create_shader(GLenum shader_type, const char *shader_path)
	{
		GLuint shader_addr = sb7::shader::load(shader_path, shader_type, true);
		if (shader_addr == 0)
			std::cerr << "쉐이더 로드 실패 : " << shader_path << std::endl;
		return shader_addr;
	}

	GLuint create_program()
	{
		GLuint program_addr = glCreateProgram();
		GLuint vshader = create_shader(GL_VERTEX_SHADER, vs_path);
		GLuint fshader = create_shader(GL_FRAGMENT_SHADER, fs_path);

		glAttachShader(program_addr, vshader);
		glAttachShader(program_addr, fshader);

		glLinkProgram(program_addr);

		glDeleteShader(vshader);
		glDeleteShader(fshader);
		return program_addr;
	}

  public:
	virtual void startup() override
	{
		programAddr = create_program();
		models.push_back(std::make_unique<Cube>(vmath::vec4(-0.5, 0, -0.5, 0)));
		camera.SetPosition(vmath::vec3(0.0f, 1.0f, 3.0f));
	}

	virtual void render(double currentTime) override
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_CULL_FACE);

		// 자전: Y축 회전
		float angle = vmath::degrees((float)currentTime * 2);
		models.back()->SetRotation(vmath::vec3(0.0f, angle, 0.0f));

		// 공전: 원점 중심 원운동
		models.back()->SetPosition(vmath::vec3((float)cos(currentTime * 2), 0.0f,
							   (float)sin(currentTime * 2)));

		glUseProgram(programAddr);
		GLuint modelMatLocation = glGetUniformLocation(programAddr, "modelMat");
		GLuint lookatMatLocation = glGetUniformLocation(programAddr, "lookatMat");
		GLuint projMatLocation = glGetUniformLocation(programAddr, "projMat");
		// Transformer가 T * Ry * Rx * Rz * S 순서로 합성해 준다
		glUniformMatrix4fv(modelMatLocation, 1, GL_FALSE,
				   models.back()->GetModelMatrix());
		glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE,
				   camera.GetLookAtMatrix());
		glUniformMatrix4fv(
			projMatLocation, 1, GL_FALSE,
			camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

		// 반례, 하면 안되는 코드‼️❌
		// my_application::vaoAddr(= 0)을 바인딩하면 빈 VAO를 쓰게 된다.
		// 실제 VAO는 Cube 생성자에서 만들어진 Cube::mVaoAddr에 저장되어 있다.
		// glBindVertexArray(vaoAddr);  // ❌ 항상 0

		glBindVertexArray(models.back()->GetVaoAddr());
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	virtual void shutdown() override
	{
	}
};
} // namespace chapter6

DECLARE_MAIN(chapter6::my_application);
