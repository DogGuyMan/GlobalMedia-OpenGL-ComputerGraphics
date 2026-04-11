#include <cmath>
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

class Transformer : public ITransformable
{
  private:
	vmath::vec3 mPosition;
	vmath::vec3 mEulerAngles; // (pitch X, yaw Y, roll Z) 단위: degree
	vmath::vec3 mScale;

  public:
	Transformer()
	    : mPosition(0.0f, 0.0f, 0.0f),
	      mEulerAngles(0.0f, 0.0f, 0.0f), mScale(1.0f, 1.0f, 1.0f)
	{
	}
	virtual ~Transformer() = default;

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

	virtual vmath::vec3 GetPosition() const override { return mPosition; }
	virtual void SetPosition(vmath::vec3 t) override { mPosition = t; }
	virtual void Translate(vmath::vec3 toward) override { mPosition += toward; }

	virtual vmath::vec3 GetRotation() const override { return mEulerAngles; }
	virtual void SetRotation(vmath::vec3 euler) override { mEulerAngles = euler; }
	virtual void Rotate(float angle, vmath::vec3 axis) override
	{
		// 축 방향에 해당하는 오일러 성분에 누적
		mEulerAngles += axis * angle;
	}

	virtual vmath::vec3 GetScale() const override { return mScale; }
	virtual void SetScale(vmath::vec3 s) override { mScale = s; }
	virtual void Scale(vmath::vec3 adj) override { mScale += adj; }
};

// 면 색상 6색 순환
static const vmath::vec4 mBaseColors[6] = {
    vmath::vec4(1.0f, 0.0f, 0.0f, 1.0f),
    vmath::vec4(0.0f, 1.0f, 0.0f, 1.0f),
    vmath::vec4(0.0f, 0.0f, 1.0f, 1.0f),
    vmath::vec4(0.0f, 1.0f, 1.0f, 1.0f),
    vmath::vec4(1.0f, 0.0f, 1.0f, 1.0f),
    vmath::vec4(1.0f, 1.0f, 0.0f, 1.0f)};

class Model
{
  protected:
	GLuint mVaoAddr;
	GLuint mVboAddr;
	GLsizei mVertexCount = 0;
	std::vector<GLfloat> mModelData;
	Transformer mTransformer;
	vmath::vec4 mOffset;

  public:
	Model(vmath::vec4 offset = vmath::vec4(0, 0, 0, 0))
	    : mOffset(offset)
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

	// ITransformable 구현체인 Transformer를 getter로 노출
	Transformer &GetTransform() { return mTransformer; }
	const Transformer &GetTransform() const { return mTransformer; }

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

	GLsizei GetVertexCount() const
	{
		return mVertexCount;
	}

  protected:
	// 서브클래스가 mModelData를 채우는 가상 메서드
	virtual void initModelData() = 0;

	// 쿼드(정점 4개) -> 삼각형 2개(정점 6개)로 삼각형화하여 mModelData에 push
	// verts[0]~[3] = CCW 순서의 쿼드 꼭짓점, w=1.0은 point를 의미
	// 삼각형 1: verts[0], verts[1], verts[2]
	// 삼각형 2: verts[0], verts[2], verts[3]
	void pushQuad(const vmath::vec4 verts[4], size_t colorIdx)
	{
		const int tri[6] = {0, 1, 2, 0, 2, 3};
		for (int t = 0; t < 6; t++)
		{
			const vmath::vec4 &v = verts[tri[t]];
			for (int j = 0; j < 4; j++)
				mModelData.push_back(v[j] + mOffset[j]);
			for (int j = 0; j < 4; j++)
				mModelData.push_back(mBaseColors[colorIdx][j]);
		}
	}

	// initModelData() 호출 후 GPU 업로드 + attribute 설정 + VAO 언바인드
	// 서브클래스 생성자(leaf class)에서 호출할 것
	// (순수 가상 함수가 포함되므로 Model 생성자에서 호출 불가)
	void build()
	{
		initModelData();
		mVertexCount = static_cast<GLsizei>(mModelData.size() / 8);

		glBufferData(GL_ARRAY_BUFFER, mModelData.size() * sizeof(GLfloat),
		             mModelData.data(), GL_STATIC_DRAW);

		GLuint stride = 8 * sizeof(float);

		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)0);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride,
		                      (void *)(4 * sizeof(float)));
		glEnableVertexAttribArray(1);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
};

class Cube : public Model
{
  private:
	// mBaseVertices[z][y][x] -> (x, y, z)
	// 반례, 하면 안되는 코드‼️❌
	// [1][1][0]이 (0,0,1)로 되어 있었음 -> y가 1이어야 하는데 0
	// [1][1][1]이 (0,1,1)로 되어 있었음 -> x가 1이어야 하는데 0
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
	         vmath::vec4(0.0, 1.0, 1.0, 1.0), // [1][1][0] = (0,1,1) <- y 수정
	         vmath::vec4(1.0, 1.0, 1.0, 1.0), // [1][1][1] = (1,1,1) <- x 수정
	     }}};

	// 반례, 하면 안되는 코드‼️❌
	// 컴포넌트 단위로 교차 배치하면 레이아웃이 [vx,cx,vy,cy,vz,cz,vw,cw]이 되어
	// glVertexAttribPointer가 연속 4개 float를 읽을 때 엉뚱한 값을 읽게 된다.
	// for(int i = 0; i < 4; i++) {
	//	 mModelData.push_back(mBaseVertices[...][i] + mOffset[i]);
	//	 mModelData.push_back(mBaseColors[0][i]);
	// }

	// 각 면은 해당 평면의 정점만 사용, CCW winding (법선이 바깥을 향함)
	void initModelData() override
	{
		// 각 면: 바깥에서 봤을 때 CCW 순서 -> front face
		// pushQuad 내부에서 {0,1,2, 0,2,3} 삼각형화

		// 앞면 (z=1, +Z 법선) — 카메라를 향하는 면
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[1][0][0],  // (0,0,1)
			    mBaseVertices[1][0][1],  // (1,0,1)
			    mBaseVertices[1][1][1],  // (1,1,1)
			    mBaseVertices[1][1][0]}; // (0,1,1)
			pushQuad(verts, 0);
		}

		// 뒷면 (z=0, -Z 법선)
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[0][0][0],  // (0,0,0)
			    mBaseVertices[0][1][0],  // (0,1,0)
			    mBaseVertices[0][1][1],  // (1,1,0)
			    mBaseVertices[0][0][1]}; // (1,0,0)
			pushQuad(verts, 1);
		}

		// 윗면 (y=1, +Y 법선)
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[0][1][0],  // (0,1,0)
			    mBaseVertices[1][1][0],  // (0,1,1)
			    mBaseVertices[1][1][1],  // (1,1,1)
			    mBaseVertices[0][1][1]}; // (1,1,0)
			pushQuad(verts, 2);
		}

		// 아래면 (y=0, -Y 법선)
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[0][0][0],  // (0,0,0)
			    mBaseVertices[0][0][1],  // (1,0,0)
			    mBaseVertices[1][0][1],  // (1,0,1)
			    mBaseVertices[1][0][0]}; // (0,0,1)
			pushQuad(verts, 3);
		}

		// 오른면 (x=1, +X 법선)
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[0][0][1],  // (1,0,0)
			    mBaseVertices[0][1][1],  // (1,1,0)
			    mBaseVertices[1][1][1],  // (1,1,1)
			    mBaseVertices[1][0][1]}; // (1,0,1)
			pushQuad(verts, 4);
		}

		// 왼면 (x=0, -X 법선)
		{
			const vmath::vec4 verts[4] = {
			    mBaseVertices[0][0][0],  // (0,0,0)
			    mBaseVertices[1][0][0],  // (0,0,1)
			    mBaseVertices[1][1][0],  // (0,1,1)
			    mBaseVertices[0][1][0]}; // (0,1,0)
			pushQuad(verts, 5);
		}
	}

  public:
	Cube(vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : Model(_offset)
	{
		build();
	};

	virtual ~Cube()
	{
	}
};

// 매개변수 곡면 — f(u,v) -> vec3 함수로 메쉬를 생성하는 Model 서브클래스
// 반례, 하면 안되는 코드‼️❌ (기존 ParametricGeometry의 버그 목록)
// 1) push_data() -> push_back() (컴파일 안 됨)
// 2) vArray[i+1][j+0] — 1D vector에 2D 인덱싱 (범위 초과)
// 3) u/v 범위가 0부터 시작 (u.first 무시)
// 4) genFunction() 생성자에서 미호출
// 5) GPU 업로드 없음 (glBufferData 없음)
// 6) VAO/VBO 생성/설정 없음
// 7) Model 미상속 -> transform/render 불가
// 8) position/color 별도 vector, 인터리브 안 됨
// 9) 루프 i < u_resolution에서 [i+1] 접근 -> off-by-one
class ParametricGeometry : public Model
{
  private:
	std::pair<double, double> mRangeU;
	std::pair<double, double> mRangeV;
	size_t mResU;
	size_t mResV;
	double mDeltaU;
	double mDeltaV;

	// (u,v) 격자로 곡면을 샘플링하여 삼각형 메쉬 생성
	void initModelData() override
	{
		// 격자점: (mResU + 1) × (mResV + 1) 개
		const size_t cols = mResV + 1;
		const size_t rows = mResU + 1;
		std::vector<std::vector<vmath::vec4>> grid(rows, std::vector<vmath::vec4>(cols));

		for (size_t i = 0; i < rows; i++)
		{
			for (size_t j = 0; j < cols; j++)
			{
				double u = mRangeU.first + i * mDeltaU;
				double v = mRangeV.first + j * mDeltaV;
				grid[i][j] = SurfaceFunction(u, v);
			}
		}

		// 각 쿼드 -> 삼각형 2개 -> 정점 6개
		size_t quadIndex = 0;
		for (size_t i = 0; i < mResU; i++)
		{
			for (size_t j = 0; j < mResV; j++)
			{
				{
					// pushQuad {0,1,2, 0,2,3}과 호환되는 CCW 순서
					//   [3]---[2]
					//    |     |
					//   [0]---[1]
					const vmath::vec4 verts[4] = {
					    grid[i + 0][j + 0],   // [0] = (u,   v)
					    grid[i + 1][j + 0],   // [1] = (u+1, v)
					    grid[i + 1][j + 1],   // [2] = (u+1, v+1)
					    grid[i + 0][j + 1],   // [3] = (u,   v+1)
					};
					quadIndex++;
					pushQuad(verts, quadIndex % 3);
				}
			}
		}
	}

  public:
	ParametricGeometry(
	    double u_start, double u_end, size_t u_res,
	    double v_start, double v_end, size_t v_res,
	    vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : Model(_offset), mRangeU(u_start, u_end), mRangeV(v_start, v_end),
	      mResU(u_res), mResV(v_res)
	{
		mDeltaU = (mRangeU.second - mRangeU.first) / mResU;
		mDeltaV = (mRangeV.second - mRangeV.first) / mResV;
		// 주의: 여기서 initModelData()를 호출하면 안 됨
		// SurfaceFunction()이 순수 가상 함수이므로 서브클래스 생성자에서 build() 호출
	}

	virtual ~ParametricGeometry()
	{
	}

	virtual vmath::vec4 SurfaceFunction(double u, double v) const = 0;
};

class Plane : public ParametricGeometry
{
  public:
	Plane(double u_start, double u_end, size_t u_res,
	      double v_start, double v_end, size_t v_res,
	      vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : ParametricGeometry(u_start, u_end, u_res, v_start, v_end, v_res, _offset)
	{
		build();
	}

	virtual vmath::vec4 SurfaceFunction(double u, double v) const override {
		return vmath::vec4(u, v, 0.0, 1.0);
	};
};

class Sphere : public ParametricGeometry
{
  public:
	Sphere(double u_start, double u_end, size_t u_res,
	       double v_start, double v_end, size_t v_res,
	       vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : ParametricGeometry(u_start, u_end, u_res, v_start, v_end, v_res, _offset)
	{
		build();
	}
	virtual vmath::vec4 SurfaceFunction(double u, double v) const override
	{
		float r = 1.0f;
		return vmath::vec4(
		    r * sinf((float)v) * cosf((float)u),
		    r * cosf((float)v),
		    r * sinf((float)v) * sinf((float)u),
		    1.0);
	}
};

// 원기둥 (옆면만)
// u: 경도 [0, 2pi] — 원주 회전, v: 높이 [0, 1] — 아래에서 위
class Cylinder : public ParametricGeometry
{
  private:
	float mRadius;
	float mHeight;

  public:
	Cylinder(double u_start, double u_end, size_t u_res,
	         double v_start, double v_end, size_t v_res,
	         float radius = 1.0f, float height = 2.0f,
	         vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : ParametricGeometry(u_start, u_end, u_res, v_start, v_end, v_res, _offset),
	      mRadius(radius), mHeight(height)
	{
		build();
	}

	virtual vmath::vec4 SurfaceFunction(double u, double v) const override
	{
		float x = mRadius * cosf((float)u);
		float y = mHeight * (float)v - mHeight / 2.0f; // 중심 기준 [-h/2, h/2]
		// -sin(u): v가 +Y 방향으로 증가하므로 회전 방향을 반전해야
		// Sphere(v가 -Y 방향)와 동일한 외향 법선 CCW winding이 됨
		float z = -mRadius * sinf((float)u);
		return vmath::vec4(x, y, z, 1.0f);
	}
};

// 원판 (뚜껑)
// u: 경도 [0, 2pi] — 원주 회전, v: 반지름 비율 [0, 1] — 중심에서 바깥
class Disk : public ParametricGeometry
{
  private:
	float mRadius;
	float mY;     // 고정 높이
	float mFlip;  // +1.0 = 윗면(+Y 법선), -1.0 = 아랫면(-Y 법선)

  public:
	Disk(double u_start, double u_end, size_t u_res,
	     double v_start, double v_end, size_t v_res,
	     float radius = 1.0f, float y = 0.0f, float flip = 1.0f,
	     vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : ParametricGeometry(u_start, u_end, u_res, v_start, v_end, v_res, _offset),
	      mRadius(radius), mY(y), mFlip(flip)
	{
		build();
	}

	virtual vmath::vec4 SurfaceFunction(double u, double v) const override
	{
		float r = mRadius * (float)v;
		// v가 중심->바깥(+방향)이므로 -sin(u)으로 외향 법선 맞춤
		// flip: +1 = 윗면(+Y 법선), -1 = 아랫면(-Y 법선) — x 반전으로 winding 제어
		float x = r * cosf((float)u) * mFlip;
		float z = -r * sinf((float)u);
		return vmath::vec4(x, mY, z, 1.0f);
	}
};

// 완전한 원기둥 (옆면 + 윗뚜껑 + 아랫뚜껑)
// 3개의 Model을 내부에서 조합하여 하나의 VAO로 렌더링
class CappedCylinder : public Model
{
  private:
	float mRadius;
	float mHeight;
	size_t mURes;
	size_t mVRes;

	// Cylinder/Disk의 SurfaceFunction을 인라인으로 사용
	void initModelData() override
	{
		float halfH = mHeight / 2.0f;

		// === 옆면 (-sin으로 외향 법선) ===
		for (size_t i = 0; i < mURes; i++)
		{
			for (size_t j = 0; j < mVRes; j++)
			{
				float u0 = 2.0f * (float)M_PI * i / mURes;
				float u1 = 2.0f * (float)M_PI * (i + 1) / mURes;
				float v0 = (float)j / mVRes;
				float v1 = (float)(j + 1) / mVRes;

				const vmath::vec4 verts[4] = {
				    vmath::vec4(mRadius * cosf(u0), mHeight * v0 - halfH, -mRadius * sinf(u0), 1.0f),
				    vmath::vec4(mRadius * cosf(u1), mHeight * v0 - halfH, -mRadius * sinf(u1), 1.0f),
				    vmath::vec4(mRadius * cosf(u1), mHeight * v1 - halfH, -mRadius * sinf(u1), 1.0f),
				    vmath::vec4(mRadius * cosf(u0), mHeight * v1 - halfH, -mRadius * sinf(u0), 1.0f),
				};
				pushQuad(verts, (i * mVRes + j) % 6);
			}
		}

		// === 윗뚜껑 (y = +halfH, +Y 법선) ===
		for (size_t i = 0; i < mURes; i++)
		{
			float u0 = 2.0f * (float)M_PI * i / mURes;
			float u1 = 2.0f * (float)M_PI * (i + 1) / mURes;

			const vmath::vec4 center(0.0f, halfH, 0.0f, 1.0f);
			const vmath::vec4 edge0(mRadius * cosf(u0), halfH, -mRadius * sinf(u0), 1.0f);
			const vmath::vec4 edge1(mRadius * cosf(u1), halfH, -mRadius * sinf(u1), 1.0f);

			// 위에서 봤을 때 CCW: center -> edge0 -> edge1
			const vmath::vec4 tri[4] = {center, edge0, edge1, center};
			pushQuad(tri, 2);
		}

		// === 아랫뚜껑 (y = -halfH, -Y 법선) ===
		for (size_t i = 0; i < mURes; i++)
		{
			float u0 = 2.0f * (float)M_PI * i / mURes;
			float u1 = 2.0f * (float)M_PI * (i + 1) / mURes;

			const vmath::vec4 center(0.0f, -halfH, 0.0f, 1.0f);
			const vmath::vec4 edge0(mRadius * cosf(u0), -halfH, -mRadius * sinf(u0), 1.0f);
			const vmath::vec4 edge1(mRadius * cosf(u1), -halfH, -mRadius * sinf(u1), 1.0f);

			// 아래에서 봤을 때 CCW: center -> edge1 -> edge0
			const vmath::vec4 tri[4] = {center, edge1, edge0, center};
			pushQuad(tri, 3);
		}
	}

  public:
	CappedCylinder(float radius = 1.0f, float height = 2.0f,
	               size_t uRes = 32, size_t vRes = 1,
	               vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : Model(_offset), mRadius(radius), mHeight(height), mURes(uRes), mVRes(vRes)
	{
		build();
	}

	virtual ~CappedCylinder() {}
};

// 물방울 형태
// paulbourke.net/geometry/teardrop + CJ01_WaterShader.pdf 통합
// Bourke 공식: horizontalR = 0.5 * (1 - cos(θ)) * sin(θ), height = cos(θ)
// fatness 파라미터로 수평 팽창 정도 제어
// u: 경도 [0,  pi] — Y축 회전, v: 위도 [0, pi] — 꼭대기에서 바닥
class WaterDrop : public ParametricGeometry
{
  private:
	float mRadius;
	float mFatness; // 수평 팽창 계수 (Bourke 기본값 0.5)

  public:
	WaterDrop(double u_start, double u_end, size_t u_res,
	          double v_start, double v_end, size_t v_res,
	          float radius = 1.0f, float fatness = 0.5f,
	          vmath::vec4 _offset = vmath::vec4(0, 0, 0, 0))
	    : ParametricGeometry(u_start, u_end, u_res, v_start, v_end, v_res, _offset),
	      mRadius(radius), mFatness(fatness)
	{
		build();
	}

	virtual vmath::vec4 SurfaceFunction(double u, double v) const override
	{
		float R = mRadius;
		float theta = (float)v; // [0, pi] 꼭대기->바닥
		float phi = (float)u;   // [0,  pi] Y축 회전

		// Bourke: (1 - cos(θ)) * sin(θ)
		// θ=0 -> 0 (뾰족한 꼭대기), θ≈2.2 -> 최대 팽창, θ=*pi -> 0 (바닥 점)
		float horizontalR = R * mFatness * (1.0f - cosf(theta)) * sinf(theta);

		float x = horizontalR * cosf(phi);
		float y = R * cosf(theta); // 높이: R(꼭대기) -> -R(바닥)
		float z = horizontalR * sinf(phi);

		return vmath::vec4(x, y, z, 1.0f);
	}
};

// 반례, 하면 안되는 코드‼️❌
// eye, target을 별도 멤버로 저장하고 ITransformable 구현을 빈 껍데기로 두면
// Model과 Camera의 변환 방식이 일관되지 않는다.
// Transformer를 공유하면 position = eye, rotation -> forward 방향 도출로
// target을 자동 계산할 수 있다.

class Camera
{
  private:
	Transformer mTransformer;
	vmath::vec3 mWorldUp;

	bool mIsPerspective = true;
	float fov;
	float nearPlane;
	float farPlane;

  public:
	Camera()
	    : mWorldUp(0.0, 1.0, 0.0),
	      fov(50), nearPlane(0.1), farPlane(1000.0)
	{
	}

	// ITransformable 구현체인 Transformer를 getter로 노출
	Transformer &GetTransform() { return mTransformer; }
	const Transformer &GetTransform() const { return mTransformer; }

	// target 지점을 바라보도록 회전 설정 (position + rotation ↔ target 상호 변환)
	void LookAt(vmath::vec3 target)
	{
		vmath::vec3 dir = vmath::normalize(target - mTransformer.GetPosition());
		// forward = Ry * Rx * (0,0,-1) = (-sin(yaw)*cos(pitch), sin(pitch), -cos(yaw)*cos(pitch))
		// 역산: pitch = asin(dir.y), yaw = atan2(-dir.x, -dir.z)
		float pitch = vmath::degrees(asinf(dir[1]));
		float yaw = vmath::degrees(atan2f(-dir[0], -dir[2]));
		mTransformer.SetRotation(vmath::vec3(pitch, yaw, 0.0f));
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

	// euler 회전으로부터 전방 벡터 계산: Ry * Rx * (0,0,-1)
	vmath::vec3 GetForward() const
	{
		vmath::vec3 euler = mTransformer.GetRotation();
		float pitch = vmath::radians(euler[0]);
		float yaw = vmath::radians(euler[1]);
		return vmath::vec3(
		    -sinf(yaw) * cosf(pitch),
		    sinf(pitch),
		    -cosf(yaw) * cosf(pitch));
	}

	vmath::vec3 GetEye() const
	{
		return mTransformer.GetPosition();
	}
	vmath::vec3 GetTarget() const
	{
		return GetEye() + GetForward();
	}

	vmath::mat4 GetViewMatrix() const
	{
		return vmath::lookat(GetEye(), GetTarget(), mWorldUp);
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

	void testModelRotate(double currentTime)
	{
		// 자전: Y축 회전
		float angle = vmath::degrees((float)currentTime * 2);

		models.back()->GetTransform().SetRotation(vmath::vec3(0.0f, angle, 0.0f));

		// 공전: 원점 중심 원운동
		models.back()->GetTransform().SetPosition(vmath::vec3(
		    (float)cos(currentTime * 2),
		    0.0f,
		    (float)sin(currentTime * 2)));
	}

	void testCameraRotate(double currentTime)
	{
		float angle = vmath::degrees((float)currentTime * 2);
		camera.GetTransform().SetPosition(vmath::vec3(
		    (float)cos(currentTime * 2) * 5,
		    1,
		    (float)sin(currentTime * 2) * 5));
		camera.LookAt(vmath::vec3(0.0, 0.0, 0.0));
	}

  public:
	virtual void startup() override
	{
		programAddr = create_program();
		// 구 매개변수 곡면: f(u,v) -> (x,y,z), u : [0, pi], v : [0, pi]

		// models.push_back(std::make_unique<Sphere>(
		//     0.0, 2.0 * M_PI, 32,
		//     0.0, M_PI, 16));
		
		// models.push_back(std::make_unique<Plane>(
		//     0.0, 1.0, 16,
		//     0.0, 1.0, 16,
		//     vmath::vec4{-0.5, -0.5, 0, 0}
		// ));

		// models.push_back(std::make_unique<Cube>(vmath::vec4{-0.5, -0.5, -0.5, 0}));

		// 물방울: u : [0, pi], v : [0, pi], fatness=0.5
		// models.push_back(std::make_unique<WaterDrop>(
		//     0.0, 2.0 * M_PI, 32,
		//     0.5, M_PI, 16,
		//     1.0f, 0.5f, vmath::vec4(0.0, 1, 0.0, 0.0)));
		// models.back()->GetTransform().SetPosition(vmath::vec3(0,-1, 0));
		models.push_back(std::make_unique<CappedCylinder>(1.0f, sqrt(2.0), 4, 1));

		camera.SetPerspective();
		camera.GetTransform().SetPosition(vmath::vec3(0.0f, 1.0f, 3.0f));
	}

	virtual void render(double currentTime) override
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_CULL_FACE);

		testCameraRotate(currentTime);

		glUseProgram(programAddr);
		GLuint modelMatLocation = glGetUniformLocation(programAddr, "modelMat");
		GLuint lookatMatLocation = glGetUniformLocation(programAddr, "lookatMat");
		GLuint projMatLocation = glGetUniformLocation(programAddr, "projMat");
		// Transformer가 T * Ry * Rx * Rz * S 순서로 합성해 준다
		glUniformMatrix4fv(modelMatLocation, 1, GL_FALSE,
		                   models.back()->GetModelMatrix());
		glUniformMatrix4fv(lookatMatLocation, 1, GL_FALSE,
		                   camera.GetViewMatrix());
		glUniformMatrix4fv(
		    projMatLocation, 1, GL_FALSE,
		    camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

		// 반례, 하면 안되는 코드‼️❌
		// my_application::vaoAddr(= 0)을 바인딩하면 빈 VAO를 쓰게 된다.
		// 실제 VAO는 Cube 생성자에서 만들어진 Cube::mVaoAddr에 저장되어 있다.
		// glBindVertexArray(vaoAddr);  // ❌ 항상 0

		glBindVertexArray(models.back()->GetVaoAddr());
		glDrawArrays(GL_TRIANGLES, 0, models.back()->GetVertexCount());
	}

	virtual void shutdown() override
	{
	}
};
} // namespace chapter6

DECLARE_MAIN(chapter6::my_application);
