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

// uniform location 캐시를 위해 Model::ModelBase가 Program::ProgramBase&를 받아야 함
// Model 네임스페이스가 Program보다 먼저 정의되므로 전방선언 필요
namespace Chapter7::Program
{
	class ProgramBase;
}

/*********************************************************************************
 *
 * SURFACES — Data Oriented 접근
 *
 * 형태를 클래스 상속(PlaneModel, SphereModel 등)이 아닌 *전역 함수 포인터*로 결정
 * ModelBase가 std::function<vmath::vec4(double u, double v)>을 받아 격자 샘플링
 *
 * === Normal 방향 규약 (오른손 좌표계) ===
 *   각 parametric surface의 normal = ∂f/∂u × ∂f/∂v  (CCW winding 기준)
 *
 *   규칙 : 수평/평평한 surface는 **default로 +Y를 향함** (-Y 금지)
 *     - Plane (XY-plane) : normal = +Z  (수직면, +Y/-Y 무관)
 *     - Sphere           : normal = position radial outward (위치 의존)
 *     - Disk             : normal = +Y  ← 수평이므로 +Y up이 default
 *     - Cylinder 옆면    : normal = radial outward (XZ 방향, +Y 성분 0)
 *     - Cone 옆면        : normal = radial outward + 기울임 (+Y 성분 있음)
 *
 *   아래 방향(-Y)이 필요한 경우 (ClosedCylinder 아래 뚜껑 등)는
 *   SurfacePart.winding = Winding::CW 로 반전 처리
 *
 *********************************************************************************/
namespace Chapter7::Surfaces
{
	using SurfaceFunction = std::function<vmath::vec4(double u, double v)>;

	// 평면 (XY-plane) : (u, v, 0)
	//   u ∈ [0, 1] 가로, v ∈ [0, 1] 세로
	//   N = ∂f/∂u × ∂f/∂v = (1,0,0) × (0,1,0) = (0, 0, +1) -> +Z (화면 바깥)
	inline vmath::vec4 Plane(double u, double v)
	{
		return vmath::vec4((float)u, (float)v, 0.0f, 1.0f);
	}

	// 구 : outward radial normal
	//   u ∈ [0, 2π] 경도, v ∈ [0, π] 위도
	//   v=0이 북극(+Y), v=π가 남극(-Y), 각 정점 normal은 radial
	inline vmath::vec4 Sphere(double u, double v)
	{
		float r = 1.0f;
		return vmath::vec4(
		    r * sinf((float)v) * cosf((float)u),
		    r * cosf((float)v),
		    r * sinf((float)v) * sinf((float)u),
		    1.0f);
	}

	// 원판 (Disk) : 평면 원, y=0 고정, **+Y normal (default 위쪽 향함)**
	//   u : 각도 [0, 2π]
	//   v : 반경 비율 [0, 1]  (0 = 중심, 1 = 가장자리)
	//   normal 계산 :
	//     ∂f/∂u = (-v·sin(u), 0, +v·cos(u))
	//     ∂f/∂v = ( cos(u),   0,  sin(u) )
	//     ∂f/∂u × ∂f/∂v = (0, +v, 0) -> +Y ✓
	//   특이점 : v=0에서 모든 정점이 원점으로 붕괴 -> degenerate triangles (렌더 문제 없음)
	//   주의 : +sin 사용으로 u-회전 방향이 Cylinder(-sin)와 반대 ->
	//          ClosedCylinder 합성 시 rim 원은 동일하나 u-파라미터가 reverse됨 (시각적으로 무관)
	inline vmath::vec4 Disk(double u, double v)
	{
		float r_max = 1.0f;
		float r = r_max * (float)v;
		return vmath::vec4(
		    r * cosf((float)u),
		    0.0f,
		    r * sinf((float)u), // +sin (이전 -sin에서 수정 — normal이 +Y가 되도록)
		    1.0f);
	}

	// 원기둥 옆면 : u ∈ [0, 2π] 경도, v ∈ [0, 1] 높이
	inline vmath::vec4 Cylinder(double u, double v)
	{
		float r = 1.0f;
		float h = 2.0f;
		return vmath::vec4(
		    r * cosf((float)u),
		    h * (float)v - h / 2.0f,
		    -r * sinf((float)u),
		    1.0f);
	}

	// 원뿔 옆면 (Cone side) : 바닥 원 -> 꼭짓점으로 수렴
	//   u : 각도 [0, 2π]
	//   v : 높이 [0, 1]  (0 = 바닥, 1 = 꼭짓점)
	//   반경은 v에 따라 선형 감소 : r = r_max * (1 - v)
	inline vmath::vec4 Cone(double u, double v)
	{
		float r_max = 1.0f;
		float h = 2.0f;
		float r = r_max * (1.0f - (float)v);
		return vmath::vec4(
		    r * cosf((float)u),
		    h * (float)v - h / 2.0f, // 바닥(-h/2) -> 꼭짓점(+h/2)
		    -r * sinf((float)u),
		    1.0f);
	}

	// =============================================================================
	// Compound Surface — 여러 SurfaceFunction 조각을 "배열"로 합성하여 복합 모델 제작
	// =============================================================================
	// 핵심 아이디어 :
	//   - 객체 상속 없이, SurfaceFunction들을 리스트로 관리 (Data Oriented)
	//   - 각 part는 자기만의 (u, v) 격자 범위/해상도와 winding 방향을 가짐
	//   - ModelBase::initModelData가 리스트를 순차 실행하며 메쉬를 누적 (인덱스 offset 자동 처리)
	//
	// 2가지 방식으로 닫힌 mesh 제작 :
	//   (방식 1) CompoundSurface에 여러 part 합성
	//     예) Closed Cylinder = {Cylinder + Disk 위 + Disk 아래}
	//         -> 40 vertex, 120 index
	//
	//   (방식 2) 단일 SurfacePart + capStart/capEnd 플래그 (정점 추가 0개)
	//     예) Cylinder with capStart=true, capEnd=true
	//         -> v=v_start rim과 v=v_end rim을 fan triangulation으로 닫음
	//         -> 10 vertex, 36 index (75% / 70% 절감)
	//     제약 :
	//       - u-domain이 wrap해야 함 (Cylinder, Cone ✓)
	//       - Cap은 평면만 (곡면 cap 불가)
	//       - Cap 내부 UV 부정확 (cube map OK, 2D texture는 왜곡)

	// 삼각형 winding 방향 — 각 part가 CCW/CW 선택
	enum class Winding
	{
		CCW, // 기본 : 바깥쪽 normal (위쪽 cap, Cylinder/Cone 옆면, Sphere 등)
		CW   // 반대 : 아래쪽 cap처럼 winding을 뒤집어야 하는 경우
	};

	// 하나의 parametric 조각 — fn + (u, v) 범위/해상도 + winding + cap 플래그
	//   fn에 transform이 필요하면 lambda로 감싸서 넣기 (예: 위 뚜껑은 y + h/2)
	//
	//   capStart/capEnd : v=v_start / v=v_end boundary rim을 fan으로 자동 닫음
	//     - 추가 정점 없음 (기존 rim 정점만 사용)
	//     - Fan center = rim의 첫 정점 (i=0)
	//     - capStart는 reverse winding (-Y normal 가정, Cylinder/Cone 바닥용)
	//     - capEnd는 forward winding (+Y normal 가정, Cylinder 위뚜껑용)
	//     - u_res ≥ 3이어야 valid triangles 생성됨 (2면 이하는 fan 불가능)
	struct SurfacePart
	{
		SurfaceFunction fn;
		double u_start;
		double u_end;
		size_t u_res;
		double v_start;
		double v_end;
		size_t v_res;
		Winding winding = Winding::CCW;
	};

	// 여러 SurfacePart의 순차 실행 컨테이너
	using CompoundSurface = std::vector<SurfacePart>;

} // namespace Chapter7::Surfaces

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

namespace Chapter7::Meshes
{
	// Cube 하나의 텍스쳐, 서로 다른면 텍스쳐링

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
		// ! 폐기 : 이전 버전은 w=0 (direction vector)였음
		//          → homogeneous perspective divide에서 ±∞/NaN, 렌더링 안 됨
		//          OpenGL point는 반드시 w=1 (point)이어야 함
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
		        {0.0, 0.0, 0.0, 0.0},
		        {1.0, 0.0, 0.0, 0.0},
		        {1.0, 1.0, 0.0, 0.0},
		        {0.0, 1.0, 0.0, 0.0},
		    }};
		static const std::vector<GLuint> QUAD_BASE_INDICES = {
		    0, 1, 2, 0, 2, 3};

	} // namespace Plane

	namespace Cube
	{
		static const vmath::vec4 CUBE_BASE_POSITIONS[2][2][2] = {
		    {
		        {
		            {0.0, 0.0, 0.0, 0.0}, // 4
		            {1.0, 0.0, 0.0, 0.0}, // 5
		        },
		        {
		            {0.0, 1.0, 0.0, 0.0}, // 6
		            {1.0, 1.0, 0.0, 0.0}, // 7
		        },
		    },
		    {
		        {
		            {0.0, 0.0, 1.0, 0.0}, // 0
		            {1.0, 0.0, 1.0, 0.0}, // 1
		        },
		        {
		            {0.0, 1.0, 1.0, 0.0}, // 2
		            {1.0, 1.0, 1.0, 0.0}, // 3
		        },
		    }};
		static const std::vector<std::vector<vmath::vec4>> CUBE_QUADS = {
		    std::vector<vmath::vec4> // +Z
		    {
		        CUBE_BASE_POSITIONS[1][0][0],
		        CUBE_BASE_POSITIONS[1][0][1],
		        CUBE_BASE_POSITIONS[1][1][1],
		        CUBE_BASE_POSITIONS[1][1][0],
		    },
		    std::vector<vmath::vec4> // -Z
		    {
		        CUBE_BASE_POSITIONS[0][1][0],
		        CUBE_BASE_POSITIONS[1][1][0],
		        CUBE_BASE_POSITIONS[1][0][0],
		        CUBE_BASE_POSITIONS[0][0][0],
		    },
		    std::vector<vmath::vec4> // +Y
		    {
		        CUBE_BASE_POSITIONS[0][1][1],
		        CUBE_BASE_POSITIONS[1][1][1],
		        CUBE_BASE_POSITIONS[1][1][0],
		        CUBE_BASE_POSITIONS[0][1][0],
		    },
		    std::vector<vmath::vec4> // -Y
		    {
		        CUBE_BASE_POSITIONS[0][0][0],
		        CUBE_BASE_POSITIONS[1][0][0],
		        CUBE_BASE_POSITIONS[0][0][1],
		        CUBE_BASE_POSITIONS[1][0][1],
		    },
		    std::vector<vmath::vec4> // +X
		    {
		        CUBE_BASE_POSITIONS[1][0][1],
		        CUBE_BASE_POSITIONS[1][0][0],
		        CUBE_BASE_POSITIONS[1][1][0],
		        CUBE_BASE_POSITIONS[1][0][1],
		    },
		    std::vector<vmath::vec4> // -X
		    {
		        CUBE_BASE_POSITIONS[0][0][0],
		        CUBE_BASE_POSITIONS[0][0][1],
		        CUBE_BASE_POSITIONS[0][1][1],
		        CUBE_BASE_POSITIONS[0][1][0],
		    },
		};

		static const std::vector<GLuint> QUAD_BASE_INDICES[6] = {
		    {0, 1, 2, 0, 2, 3},
		};

		// ! 폐기 : inline MeshData Cube() { ... }
		// 이유 : Surfaces::Cylinder + u_res=4 로 비슷한 4면 프리즘을 만들 수 있고,
		//        cube map 전용 정점 배치는 사용자가 콘솔 출력을 기반으로 직접 편집하는 워크플로우
	} // namespace Cube
} // namespace Chapter7::Meshes

/*********************************************************************************
 *
 * HEADER
 *
 *********************************************************************************/

namespace Chapter7::Model
{

	// =============================================================================
	// Transform — 공간 변환 상태 (ModelBase에 포함)
	// =============================================================================
	//   - pivot : local 회전/스케일 중심 (Unity의 transform pivot)
	//   - translate / eulerRotate / scale : 표준 TRS
	//   - GetModelMatrix() : T * R * S * T(-pivot) 조합
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

	// =============================================================================
	// Material — 시각 상태 (ModelBase에 포함)
	// =============================================================================
	//   - baseColor : vertex color 대체 (fragment 전용, Material이 주입)
	//   - uvOffset / uvRatio : UV 변환 파라미터
	//   - TextureSlot 리스트 : 여러 샘플러(2D / CubeMap 등) 지원
	//
	// Data Oriented 설계 :
	//   - 상속 없음, 다양한 재질은 파라미터(색/UV/텍스처 슬롯)로 결정
	//   - 다중 텍스처 확장 : mTextures에 target=GL_TEXTURE_CUBE_MAP 슬롯을 추가하면 됨
	//   - 새 Material 종류를 만들 때 subclass가 아닌 "어떤 텍스처/색을 주입할지"만 다름
	class Material
	{
	  public:
		struct TextureSlot
		{
			GLuint addr;             // GL texture object handle
			GLenum target;           // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, ...
			std::string samplerName; // 셰이더 uniform 이름 (예: "tex1", "envMap")
			int unit;                // texture unit (GL_TEXTURE0 + unit)
		};

	  private:
		vmath::vec4 mBaseColor;
		vmath::vec2 mUVOffset;
		vmath::vec2 mUVRatio;
		std::vector<TextureSlot> mTextures;

		// 기본 fallback 1x1 white 2D 텍스처 (모든 Material이 공유)
		//   사용자가 AddTexture2D("tex1", 0, ...)를 호출하지 않아도 sampler2D tex1이
		//   유효한 텍스처를 읽도록 보장. baseColor * white = baseColor 렌더링 가능.
		//   사용자 텍스처가 추가되면 Apply()에서 그것이 뒤에 바인딩되어 덮어씀.
		static GLuint sDefaultWhiteTex2D;
		static void EnsureDefaultTextures();

	  public:
		Material(vmath::vec4 baseColor = vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		~Material();

		// GL 리소스 소유 -> 복사 금지, 이동만 허용
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

		// 2D 텍스처 추가
		//   samplerName : 셰이더의 uniform sampler 이름
		//   unit        : texture unit 번호 (0, 1, 2 ...)
		Material &AddTexture2D(const std::string &samplerName, const char *image_path, int unit);

		// 모든 Material 상태를 program에 적용 (glDrawElements 직전에 호출)
		void Apply(Program::ProgramBase &prog);
	};

	// =============================================================================
	// ModelBase — 기하(VAO/VBO/EBO) + Transform + Material의 컴포지션
	// =============================================================================
	// ! 폐기 : class PlaneModel : public ModelBase { ... };  — 하드코딩된 정점
	// ! 폐기 : class CubeModel : public ModelBase { ... };
	// ! 폐기 : 멤버로 pivot/translate/euler/scale/uvOffset/uvRatio/texAddr 직접 보유
	//          -> Transform / Material 객체로 분리하여 SRP 적용
	class ModelBase
	{
	  protected:
		GLuint mVAOAddr;
		GLuint mVBOAddr;
		GLuint mEBOAddr;

		// 컴포지션 : Model은 공간 상태(Transform) + 시각 상태(Material)를 "가짐"
		Transform mTransform;
		Material mMaterial;

		std::vector<GLfloat> mBufferObject;
		std::vector<GLuint> mElementBuffer;

		bool mIsBuilted = false;

		// Data Oriented : 형태를 외부에서 주입 (클래스 상속이 아닌 파라미터로)
		// 세 가지 초기화 경로 :
		//   (A) Single parametric    : SurfaceFunction 1개 + (u, v) 범위/해상도 (단일 도형)
		//   (B) Compound parametric  : 여러 SurfacePart의 리스트 (닫힌 실린더 등 합성)
		//   (C) Direct Mesh          : 미리 빌드된 raw vertex/index 데이터 (하드코딩)
		//
		// (A)는 내부적으로 1-element CompoundSurface로 변환되므로, 실제 저장되는 필드는
		//     mSurfaceParts와 mDirect* 뿐 — mUseDirectMesh로 분기
		bool mUseDirectMesh = false;
		Surfaces::CompoundSurface mSurfaceParts;
		GLsizei mIndexCount = 0;

		void initModelData();

	  public:
		// (A) Single parametric 생성자 — surface function 1개로 격자 생성 (편의 생성자)
		//     내부적으로 1-element CompoundSurface로 변환하여 (B) 경로 재사용
		ModelBase(Surfaces::SurfaceFunction surfaceFn,
		          double u_start, double u_end, size_t u_res,
		          double v_start, double v_end, size_t v_res,
		          vmath::vec3 _pivot = vmath::vec3(0.0, 0.0, 0.0));

		// (C) Direct mesh 생성자 — raw vertex/index 데이터 주입
		//     정점 레이아웃은 parametric과 동일 : pos4 + uv2 = 6 float/정점
		ModelBase(std::vector<GLfloat> vertices, std::vector<GLuint> indices,
		          vmath::vec3 _pivot = vmath::vec3(0.0, 0.0, 0.0));
		virtual ~ModelBase();

		// 컴포지션 접근자 — 공간 상태는 GetTransform(), 시각 상태는 GetMaterial()로
		Transform &GetTransform();
		const Transform &GetTransform() const;
		Material &GetMaterial();
		const Material &GetMaterial() const;

		GLuint GetVertexArrayObject() const;

		ModelBase &Build();
		void Deconstruct();
		void Draw(Program::ProgramBase &prog);

		// 개발용 : Build() 이후 mBufferObject / mElementBuffer를 콘솔에 덤프
		//   - 사용자가 콘솔 출력을 복사하여 direct-mesh 생성자 하드코딩 스크립팅에 사용
		//   - 출력 형식 :
		//       vertices : 1 vertex/line (pos4 + uv2 = 6 float)
		//       indices  : 1 quad/line (6 index = 2 triangle)
		void PrintMeshData(std::ostream &os = std::cout) const;
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
		// default : 2D texture 셰이더
		ProgramBase();
		// shader 경로를 받는 오버로드 — cube map 등 다른 셰이더로 program 생성 시
		ProgramBase(const char *vs_path, const char *fs_path);
		~ProgramBase();
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

	// =============================================================================
	// Transform 구현
	// =============================================================================
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

	// 공식 :  M = T(translate) * R * S * T(-pivot)
	//   1) T(-pivot) * v  : pivot이 원점에 오도록 (local center -> origin)
	//   2) S * v          : local center 기준 스케일
	//   3) R * v          : local center 기준 회전
	//   4) T(translate)   : 월드 위치로
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

	// =============================================================================
	// Material 구현
	// =============================================================================
	Material::Material(vmath::vec4 baseColor)
	    : mBaseColor(baseColor),
	      mUVOffset(vmath::vec2(0.0f, 0.0f)),
	      mUVRatio(vmath::vec2(1.0f, 1.0f))
	{
	}

	Material::~Material()
	{
		// 소유한 GL 텍스처 객체 해제
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

	// 2D 텍스처 추가 — 여러 번 호출하여 복수 텍스처 슬롯 구성 가능
	Material &Material::AddTexture2D(const std::string &samplerName, const char *image_path, int unit = 0)
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

	// 정적 멤버 정의
	GLuint Material::sDefaultWhiteTex2D = 0;

	// 1x1 white 2D 텍스처 lazy 생성 — 모든 Material이 공유
	//   첫 Apply() 호출 시 한 번만 생성, 이후 재사용
	//   GL 컨텍스트가 있어야 하므로 정적 초기화가 아닌 lazy init
	void Material::EnsureDefaultTextures()
	{
		if (sDefaultWhiteTex2D != 0)
			return;

		glGenTextures(1, &sDefaultWhiteTex2D);
		glBindTexture(GL_TEXTURE_2D, sDefaultWhiteTex2D);

		const unsigned char white[4] = {255, 255, 255, 255};
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	}

	// glDrawElements 직전에 호출 — 이 Material의 모든 상태를 program에 반영
	void Material::Apply(Program::ProgramBase &prog)
	{
		GLuint progAddr = prog.GetProgramAddress();

		// baseColor : vertex color 대체
		glUniform4fv(glGetUniformLocation(progAddr, "baseColor"), 1, mBaseColor);
		// UV 변환
		glUniform2fv(glGetUniformLocation(progAddr, "uvOffset"), 1, mUVOffset);
		glUniform2fv(glGetUniformLocation(progAddr, "uvRatio"), 1, mUVRatio);

		// 기본 fallback : 1x1 white 2D 텍스처를 unit 0에 바인딩 + sampler "tex1" 매핑
		//   사용자가 AddTexture2D를 호출하지 않아도 sampler2D tex1이 유효한 텍스처를 읽음
		//   -> "unit 0 GLD_TEXTURE_INDEX_2D is unloadable" 에러 방지
		//   -> baseColor * white(1,1,1,1) = baseColor 렌더링 가능
		//   사용자 텍스처는 아래 루프에서 바인딩되어 이 default를 덮어씀
		EnsureDefaultTextures();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, sDefaultWhiteTex2D);
		GLint tex1Loc = glGetUniformLocation(progAddr, "tex1");
		if (tex1Loc >= 0)
			glUniform1i(tex1Loc, 0);

		// 사용자 텍스처 슬롯 바인딩 + sampler uniform 매핑 — default를 override
		//   (sampler->unit 매핑은 Material이 여러 program과 호환되도록 매 Apply 시 설정)
		for (const auto &slot : mTextures)
		{
			glActiveTexture(GL_TEXTURE0 + slot.unit);
			glBindTexture(slot.target, slot.addr);
			glUniform1i(glGetUniformLocation(progAddr, slot.samplerName.c_str()), slot.unit);
		}
	}

	// =============================================================================
	// ModelBase 구현
	// =============================================================================
	// (A) Single parametric 생성자 — 편의용
	//   내부적으로 1-element CompoundSurface로 변환하여 (B) 경로 재사용
	ModelBase::ModelBase(
	    Surfaces::SurfaceFunction surfaceFn,
	    double u_start, double u_end, size_t u_res,
	    double v_start, double v_end, size_t v_res,
	    vmath::vec3 _pivot)
	    : mTransform(_pivot),
	      mMaterial(), // default : baseColor = (1,1,1,1)
	      mUseDirectMesh(false),
	      mSurfaceParts{Surfaces::SurfacePart{
	          std::move(surfaceFn),
	          u_start, u_end, u_res,
	          v_start, v_end, v_res,
	          Surfaces::Winding::CCW}}
	{
	}

	// (C) Direct-mesh 생성자 : 하드코딩된 raw vertex/index 주입
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

	// initModelData() : CPU only — GL 호출 0개
	//   Direct-mesh는 Build()에서 이 함수를 호출하지 않음 (생성자가 이미 mBufferObject에 복사)
	//   Compound parametric : mSurfaceParts를 순차 실행하며 인덱스 offset 누적 + winding 적용
	//     각 part의 정점/인덱스는 이전 part의 결과 뒤에 이어붙임
	void ModelBase::initModelData()
	{
		// Compound parametric 경로 : mSurfaceParts 순차 실행
		// 정점 레이아웃 : pos4 + uv2 = 6 float/정점
		mBufferObject.clear();
		mElementBuffer.clear();

		for (const auto &part : mSurfaceParts)
		{
			// 현재까지 누적된 정점 개수 — 이번 part의 인덱스 offset
			const GLuint baseIndex = (GLuint)(mBufferObject.size() / 6);

			const size_t rowCount = part.u_res + 1; // 4 + 1 = 5
			const size_t colCount = part.v_res + 1; // 4 + 1 = 5
			const double deltaU = (part.u_end - part.u_start) / (double)part.u_res;
			const double deltaV = (part.v_end - part.v_start) / (double)part.v_res;

			//
			// 정점 생성 : part.fn(u, v) -> 위치, UV는 격자 정규화
			for (size_t i = 0; i < rowCount; i++)
			{
				for (size_t j = 0; j < colCount; j++)
				{
					double u = part.u_start + (double)i * deltaU;
					double v = part.v_start + (double)j * deltaV;

					vmath::vec4 pos = part.fn(u, v);
					mBufferObject.push_back(pos[0]);
					mBufferObject.push_back(pos[1]);
					mBufferObject.push_back(pos[2]);
					mBufferObject.push_back(pos[3]);

					mBufferObject.push_back(Meshes::BASE_COLORS[0][0]);
					mBufferObject.push_back(Meshes::BASE_COLORS[0][1]);
					mBufferObject.push_back(Meshes::BASE_COLORS[0][2]);
					mBufferObject.push_back(Meshes::BASE_COLORS[0][3]);
					// UV : 정규화된 격자 좌표
					mBufferObject.push_back((float)i / (float)part.u_res);
					mBufferObject.push_back((float)j / (float)part.v_res);
				}
			}

			// EBO : 각 cell을 2개 삼각형으로 + winding 적용 + baseIndex offset
			for (size_t i = 0; i < part.u_res; i++)
			{

				for (size_t j = 0; j < part.v_res; j++)
				{
					GLuint idx00 = baseIndex + (GLuint)(i * colCount + j);
					GLuint idx10 = baseIndex + (GLuint)((i + 1) * colCount + j);
					GLuint idx11 = baseIndex + (GLuint)((i + 1) * colCount + (j + 1));
					GLuint idx01 = baseIndex + (GLuint)(i * colCount + (j + 1));

					if (part.winding == Surfaces::Winding::CCW)
					{
						// CCW : (i,j), (i+1,j), (i+1,j+1)  +  (i,j), (i+1,j+1), (i,j+1)
						mElementBuffer.push_back(idx00); // A
						mElementBuffer.push_back(idx10); // B
						mElementBuffer.push_back(idx11); // C
						mElementBuffer.push_back(idx00); // A
						mElementBuffer.push_back(idx11); // C
						mElementBuffer.push_back(idx01); // D
					}
					else
					{
						// CW : winding 반전 (아래쪽 cap처럼 -Y normal 원할 때)
						//      각 삼각형의 2번째/3번째 정점 순서 swap
						mElementBuffer.push_back(idx00); // A
						mElementBuffer.push_back(idx11); // C
						mElementBuffer.push_back(idx10); // B
						mElementBuffer.push_back(idx00); // A
						mElementBuffer.push_back(idx01); // D
						mElementBuffer.push_back(idx11); // C
					}
				}
			}
		}
	}

	// Build() : GPU 업로드 boilerplate
	// !!! 절대 어기면 안 되는 순서 !!!
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

		// 1) CPU 데이터 준비 (GL 호출 0개)
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

		// 2) VAO 생성/바인딩 — 이후 attribute pointer / EBO 바인딩이 VAO에 기록됨
		glGenVertexArrays(1, &mVAOAddr);
		glBindVertexArray(mVAOAddr);

		// 3) VBO 생성 -> 바인딩 -> 업로드
		glGenBuffers(1, &mVBOAddr);
		glBindBuffer(GL_ARRAY_BUFFER, mVBOAddr);
		glBufferData(GL_ARRAY_BUFFER, mBufferObject.size() * sizeof(GLfloat), mBufferObject.data(), GL_STATIC_DRAW);

		// 4) EBO 생성 -> 바인딩 -> 업로드 (VAO에 기록)
		glGenBuffers(1, &mEBOAddr);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBOAddr);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, mElementBuffer.size() * sizeof(GLuint), mElementBuffer.data(), GL_STATIC_DRAW);

		// 5) attribute layout (VAO에 기록)

		GLuint stride = 10 * sizeof(GLfloat);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void *)(0)); // position  // ! 함수 이름 외우기
		glEnableVertexAttribArray(0);

		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, (void *)(4 * sizeof(float))); // color        // ! 함수 이름 외우기
		glEnableVertexAttribArray(1);

		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void *)(8 * sizeof(float))); // uv        // ! 함수 이름 외우기
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
		// 1) VAO 바인딩 (기하)
		glBindVertexArray(mVAOAddr);

		// 2) Transform uniform : modelMat
		glUniformMatrix4fv(glGetUniformLocation(prog.GetProgramAddress(), "modelMat"),
		                   1, false, mTransform.GetModelMatrix());

		// 3) Material 적용 : baseColor, uvOffset, uvRatio, 모든 텍스처 바인딩
		//    (모델별 uniform은 glDrawElements 직전에 "한 묶음"으로 set해야 다른 모델과 섞이지 않음)

		mMaterial.Apply(prog);

		// 4) Draw
		glDrawElements(GL_TRIANGLES, mIndexCount, GL_UNSIGNED_INT, 0);
	}

	// 개발용 덤프 — Build() 이후 호출
	//   format (사용자가 콘솔 출력을 복사하여 direct-mesh 생성자 하드코딩 스크립팅에 사용) :
	//     vertices : 1 vertex/line (pos4 + uv2 = 6 float)
	//     indices  : 1 quad/line (6 index = 2 triangle)
	//   snprintf 사용 — std::cout 포맷 flag에 부작용 주지 않음
	void ModelBase::PrintMeshData(std::ostream &os) const
	{
		const size_t floatsPerVertex = 10; // pos4 + uv2
		const size_t indicesPerQuad = 10;  // 2 triangle × 3 vertex

		const size_t vertexCount = mBufferObject.size() / floatsPerVertex;
		const size_t quadCount = mElementBuffer.size() / indicesPerQuad;

		os << "// ===== ModelBase::PrintMeshData =====\n";
		os << "// layout  : vertex = pos(x,y,z,w) + color(x,y,z,w) + uv(u,v)  [10 float/vertex]\n";
		os << "// vertex  : " << vertexCount << "\n";
		os << "// index   : " << mElementBuffer.size() << "  (quad = " << quadCount << ")\n";
		os << "\n";

		char buf[64];

		// vertices : 1 vertex/line
		os << "std::vector<GLfloat> vertices = {\n";
		for (size_t i = 0; i < mBufferObject.size(); i++)
		{
			snprintf(buf, sizeof(buf), "%.6ff,", mBufferObject[i]);
			os << buf;
			if ((i + 1) % floatsPerVertex == 0)
				os << "\n";
			else
				os << " ";
		}
		os << "};\n\n";

		// indices : 1 quad/line (6 indices)
		os << "std::vector<GLuint> indices = {\n";
		for (size_t i = 0; i < mElementBuffer.size(); i++)
		{
			snprintf(buf, sizeof(buf), "%u,", mElementBuffer[i]);
			os << buf;
			if ((i + 1) % indicesPerQuad == 0)
				os << "\n";
			else
				os << " ";
		}
		os << "};\n";

		os << "// ===== End Dump =====\n";
		os << std::flush; // stdout이 파이프/파일로 리다이렉트될 때 버퍼 flush 강제
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
	    : ProgramBase("./shaders/default_vs.glsl", "./shaders/default_fs.glsl")
	{
		// delegating constructor : 기본 셰이더 경로 위임
	}

	// 임의 shader 경로를 받는 생성자 — cube map 등 다른 셰이더로 program 생성 시
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
		const GLfloat backgroundColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

	  public:
		double deltaTime = 1.0 / 60;
		virtual void startup() override
		{
			programs.push_back(std::make_unique<Program::ProgramBase>());
			camera = Camera::Camera(
			    {0.0, 0.0, 2.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0},
			    60, 0.1, 1000.0);

			// stbi_set_flip_vertically_on_load(true);

			// 현재 : 단일 SurfacePart + capStart + capEnd
			//   - Cylinder 측면만 생성 후 EBO에 cap fan을 덧붙여 닫음
			//   - 정점 추가 0개, parametric 생성 결과 그대로 사용
			//   - u_res=4, v_res=1 -> 10 vertex, 24(side) + 4(top fan) + 4(bottom fan) = 32 index

			// auto cube = std::make_unique<Model::ModelBase>(
			//     Surfaces::Cylinder,
			//     0.0, 2 * PI, 4,
			//     0.0, 1, 1);
			// cube->Build();
			// // 콘솔에 정점/인덱스 덤프 — cap fan이 어떻게 추가됐는지 확인용
			// std::cout << "\n";
			// cube->PrintMeshData();
			// std::cout << "\n";

			// cube->GetMaterial().SetBaseColor(vmath::vec4(1.0, 0.0, 0.0, 1.0));
			// programs.back()->PushModel(std::move(cube));

			{
				std::vector<float> vertices;
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 4; j++)
						vertices.push_back(Meshes::Triangle::TRIANGLE_BASE_POSITIONS[1][i][j]);
					for (int c = 0; c < 4; c++)
						vertices.push_back(Meshes::BASE_COLORS[i][c]);
					for (int axis = 0; axis < 2; axis++)
						vertices.push_back(Meshes::BASE_MESH_UVS[i][axis]);
				}

				auto model = std::make_unique<Model::ModelBase>(
				    vertices,
				    Meshes::Triangle::TRIANGLE_BASE_INDICES,
				    vmath::vec3(0.5, 0.5, -0.0f));

				model->Build();
				model->PrintMeshData();
				programs.back()->PushModel(std::move(model));
			}

			{
				std::vector<float> vertices;
				for (int i = 0; i < 3; i++)
				{
					for (int j = 0; j < 4; j++)
						vertices.push_back(Meshes::Triangle::TRIANGLE_BASE_POSITIONS[0][i][j]);
					for (int c = 0; c < 4; c++)
						vertices.push_back(Meshes::BASE_COLORS[i][c]);
					for (int axis = 0; axis < 2; axis++)
						vertices.push_back(Meshes::BASE_MESH_UVS[i][axis]);
				}

				auto model = std::make_unique<Model::ModelBase>(
				    vertices,
				    Meshes::Triangle::TRIANGLE_BASE_INDICES,
				    vmath::vec3(0.5, 0.5, -0.0f));

				model->Build();
				model->PrintMeshData();
				programs.back()->PushModel(std::move(model));
			}

			{
				std::vector<float> vertices;
				for (int i = 0; i < 4; i++)
				{
					for (int j = 0; j < 4; j++)
						vertices.push_back(Meshes::Plane::QUAD_BASE_POSITIONS[0][i][j]);
					for (int c = 0; c < 4; c++)
						vertices.push_back(Meshes::BASE_COLORS[i][c]);
					for (int axis = 0; axis < 2; axis++)
						vertices.push_back(Meshes::BASE_MESH_UVS[i][axis]);
				}

				auto model = std::make_unique<Model::ModelBase>(
				    vertices,
				    Meshes::Triangle::TRIANGLE_BASE_INDICES,
				    vmath::vec3(0.5, 0.5, -0.0f));

				model->Build();
				model->PrintMeshData();
				programs.back()->PushModel(std::move(model));
			}

			// 	auto sides = std::make_unique<Model::ModelBase>(
			// 	    vertices, Meshes::Cube::QUAD_BASE_INDICES[0], vmath::vec3(-0.5, -0.5, -0.5));
			// 	sides->Build();
			// 	sides->PrintMeshData();
			// 	sides->GetMaterial().SetBaseColor(vmath::vec4(1.0,1.0,1.0,1.0));
			// 	programs.back()->PushModel(std::move(sides));
			// }
		}

		virtual void render(double currentTime) override
		{

			glClearBufferfv(GL_COLOR, 0, backgroundColor);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);

			float angle = vmath::radians((currentTime * 180) / 3.14) * 10;

			for (const auto &prog : programs)
			{
				prog->UseProgram();
				// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "modelMat"), 1, false, model->GetModelMatrix()); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "viewMat"), 1, false, camera.GetViewMatrix()); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// ! D4 : for(const auto& model : prog->GetModels()) // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// ! D4 : 	glUniformMatrix4fv(glGetUniformLocation(progAddr, "projMat"), 1, false, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight)); // 3번 별개로 순회하면 안된다 view/proj은 model 별개임
				// TODO 모델은 단일 루프로 통합, view/proj는 Draw 호출 *전*에 set해야 첫 프레임부터 정상
				// uniform location은 prog의 lazy 캐시에서 조회 (첫 프레임만 driver, 이후는 hash map hit)
				glUniformMatrix4fv(glGetUniformLocation(prog->GetProgramAddress(), "viewMat"), 1, false, camera.GetViewMatrix());
				glUniformMatrix4fv(glGetUniformLocation(prog->GetProgramAddress(), "projMat"), 1, false, camera.GetProjectionMatrix(info.windowWidth, info.windowHeight));

				auto &models = prog->GetModels();

				// cube program : 큐브에 자동 회전 적용 (6면을 볼 수 있도록)
				if (!models.empty())
				{
					float degY = (float)currentTime * 30.0f;
					float degX = (float)currentTime * 15.0f;
					models[0]->GetTransform().SetEulerRotate({degX, degY, 0.0f});
				}

				// ! 폐기 : 메인 루프 뒤에 추가 draw call로 uniform을 별도로 설정하려 했던 코드
				//          - depth test로 인해 두 번째 draw가 화면에 안 나옴
				//          - modelMat이 직전 모델 값으로 남아있어 엉뚱한 위치에 그려짐
				//          - uniform이 program 소속이라 메인 루프의 다른 모델에 영향
				// TODO 모델별 uniform은 PlaneModel::Draw() 안에서 mUVOffset/mUVRatio 멤버로 set
				for (const auto &model : prog->GetModels())
					model->Draw(*prog);
			}
		}

		virtual void shutdown() override
		{
		}
	};
}; // namespace Chapter7

DECLARE_MAIN(Chapter7::MyApplicaion);
