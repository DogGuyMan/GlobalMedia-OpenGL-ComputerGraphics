# SJH Engine — Core API Reference

> **대상 독자**: 본 프로젝트에 *참여하는 다른 AI 에이전트* + *프로젝트 개발자*.
> **목적**: 코어 라이브러리 (`src/<module>/`) 의 *공개 API* 와 *사용 컨벤션* 을 한 곳에 모음.
> **갱신 주기**: 모듈 API 변경 / 신규 컨벤션 도입 시. 핵심 컨벤션은 [메모리](../.claude/projects/-Users-escatrgot-DevelopProjects-SSU-GlobalMedia-OpenGL-ComputerGraphics/memory/) 와 일치.

---

## 목차

1. [엔진 철학](#1-엔진-철학)
2. [모듈 맵 + 의존 그래프](#2-모듈-맵--의존-그래프)
3. [코어 API — 모듈별](#3-코어-api--모듈별)
   - 3.1 [`SJH::common`](#31-sjhcommon)
   - 3.2 [`SJH::diagnostics`](#32-sjhdiagnostics)
   - 3.3 [`SJH::buffer`](#33-sjhbuffer)
   - 3.4 [`SJH::shader`](#34-sjhshader)
   - 3.5 [`SJH::program`](#35-sjhprogram)
   - 3.6 [`SJH::layout`](#36-sjhlayout)
   - 3.7 [`SJH::material`](#37-sjhmaterial)
   - 3.8 [`SJH::object`](#38-sjhobject)
   - 3.9 [`SJH::scene`](#39-sjhscene)
   - 3.10 [`SJH::render`](#310-sjhrender)
   - 3.11 [`SJH::input`](#311-sjhinput)
   - 3.12 [`SJH::resource_registry`](#312-sjhresource_registry)
4. [컨벤션](#4-컨벤션)
   - 4.1 [Compound Actor](#41-compound-actor)
   - 4.2 [Builder Pattern (Components)](#42-builder-pattern-components)
   - 4.3 [Material 셋업](#43-material-셋업)
   - 4.4 [Camera 모드 (Free / TargetLock)](#44-camera-모드-free--targetlock)
   - 4.5 [Uniform 송신의 두 layer](#45-uniform-송신의-두-layer--material-한정-vs-씬-전역)
   - 4.6 [Lighting 셰이더 schema](#46-lighting-셰이더-schema)
5. [빠른 시작 — 새 챕터](#5-빠른-시작--새-챕터)
6. [빌드 시스템](#6-빌드-시스템)

---

## 1. 엔진 철학

### Core / Client 분리 (Cocos2d-x `libcocos2d` / Unreal `Runtime` 정통)

- **Core** = `src/<module>/` 의 12 모듈. *재사용 가능한 엔진 코어*.
- **Client** = `apps/<chapter>/` 의 데모/챕터. *특정 시나리오의 양산형 코드*.
- 단일 `SJH::engine` INTERFACE 우산이 12 코어 모듈을 한 번에 link 노출.

### Actor + Component (Unity / Cocos2d-x 정통)

- **Actor 는 비상속**. `class XxxActor : public Actor` 형태 **금지**.
- 특수 속성은 *오직 Component 부착* 으로만 부여 — ECS Bundle (Bevy) 정신.
- Actor 의 정체성은 *어떤 Component 가 부착됐는지* 로만 결정.
- Compound Actor 컨벤션 — 자주 쓰이는 조합은 [`src/scene/compound_actor.h`](../src/scene/compound_actor.h) 의 free factory.

### 책임 분리 — 데이터 vs 송신

| 책임 | 담당 | 위치 |
|---|---|---|
| 셰이더 *schema* | `UniformCache` | `Program` 소유, `Material` 참조 |
| Material *값* (properties bag) | `Material` | `Floats / Ints / Vec3s / Vec4s / Mat4s / Textures` typed maps |
| Material *송신* | `MaterialApplier::Apply` | Cache outer + Material lookup inner |
| Light *값* | Component (DirLight/PointLight/SpotLight) | `src/object/light.h` |
| Light *송신* | `RenderSystem::SendLightUniforms` | 매 프레임 모든 Program 에 자동 |
| Camera view 행렬 | Camera 의 owner Actor Transform | `src/scene/camera.cpp` |

### Uniform 송신의 두 layer — Material 한정 vs 씬 전역 transient

Unity 의 *material.SetFloat* (instance) vs *Shader.SetGlobalFloat* (씬 전역) 분리 정통.

| 자유함수 family | 헤더 | 인자 | 동작 시점 | 용도 |
|---|---|---|---|---|
| `Uniforms::Set*(Material&, ...)` | [`material_uniforms.h`](../src/material/material_uniforms.h) | `Material&` | properties bag 에 **store 만** (GL 호출 없음) | Material instance 자기 슬롯 — color/texture/shininess 등 사용자 컨텐츠 |
| `Uniforms::Set*(const Program&, ...)` | [`program_uniforms.h`](../src/program/program_uniforms.h) | `const Program&` | **즉시 GL `glUniform*` 호출** | 씬 전역 transient — uModel/uView/uProj / 광원 / viewPos / MaterialApplier 의 내부 송신 경로 |

**왜 Material 만으로 모든 uniform 송신 안 되나** — `uModel` 은 *DrawCommand 마다 다른 값* (Actor.WorldMatrix). Material 에 store 하면 N draw call 마다 N×(Material 변경 + Apply 재호출) 비효율 + 동일 Material 을 여러 Actor 가 공유 시 마지막 transform 만 적용되는 의미 깨짐. 광원도 *씬 전역* 이라 모든 Material 공통 — Material 자기 데이터 아님. 따라서 *transient state* 는 RenderQueue/RenderSystem 이 *Material 우회* 로 Program 에 직접 송신.

Unity 의 `Camera` 가 매 프레임 자동 송신하는 builtin uniform 들 — 우리는 RenderSystem 이 명시 송신 (`SendLightUniforms`).

---

## 2. 모듈 맵 + 의존 그래프

```
                 ┌─────────────┐
                 │  common     │  (헤더 only, 의존 0)
                 └──────┬──────┘
                        │
        ┌───────────────┼──────────────┐
        ▼               ▼              ▼
   diagnostics       buffer         layout
                        │              │
                        ▼              ▼
                     shader         object  ── object/light.cpp 가 scene 의존 (Component base)
                        │              │
                        ▼              ▼
                     program ◄────► material   (Observer 패턴 — 양방향)
                        │              │
                        ▼              ▼
                     resource_registry          scene  (Actor / Camera / Compound Actor)
                                                  │
                                                  ▼
                                                render  (RenderSystem + RenderContext + RenderQueue)
                                                  │
                                                  ▼
                                                 input  (KeyboardInput / MouseInput)
```

| 모듈 | 종류 | 책임 (요약) |
|---|---|---|
| `SJH::common` | STATIC | 공통 상수 + 유틸 (`Const::SFX_*` 등 셰이더 schema suffix) |
| `SJH::diagnostics` | STATIC | GL 호출 / 셰이더 / uniform / 상태 / 엔진 단위 진단 |
| `SJH::buffer` | STATIC | VBO/EBO RAII (`Buffer`) + Framebuffer |
| `SJH::shader` | STATIC | 셰이더 컴파일 + InfoLog |
| `SJH::program` | STATIC | 프로그램 링크 + `UniformCache` + Material Observer 등록 |
| `SJH::layout` | STATIC | `Vertex` 구조체 + VAO + attribute setter |
| `SJH::material` | INTERFACE | Material properties bag + `Uniforms::Set*` 자유함수 |
| `SJH::object` | STATIC | Mesh + Geometry + Transform + Light Components |
| `SJH::scene` | STATIC | Actor + Component + Director + Camera + Compound Actor + MeshRenderer |
| `SJH::render` | STATIC | RenderSystem + RenderContext + RenderQueue + MaterialApplier |
| `SJH::input` | STATIC | `KeyboardInput<TAction>` + `MouseInput` |
| `SJH::resource_registry` | STATIC | Texture/Material/Model/Program/Mesh 캐시 (싱글톤) |
| `SJH::engine` | **INTERFACE** | **위 12 모듈 우산** — `target_link_libraries(... PRIVATE SJH::engine)` 한 줄 |

---

## 3. 코어 API — 모듈별

### 3.1 `SJH::common`

[`src/common/constants.h`](../src/common/constants.h) — 셰이더 schema 매크로 + UI 라벨.

**핵심 컨벤션**:
- `Const::SFX_DIRECTION = ".direction"` 등 *uniform 멤버 suffix* — `program_uniforms.cpp` 가 prefix 와 결합.
- `Const::UNI_POINT_LIGHTS_PREFIX = "pointLights["` 등 *배열 uniform prefix*.

---

### 3.2 `SJH::diagnostics`

GL 진단 4종 분담 (네임스페이스 `SJH::Diagnostics`):

| 헤더 | 책임 |
|---|---|
| `diagnostics/gl_log.h` (`GLDebug` / `GLObjectLog`) | GL 호출 직후 `glGetError`, 셰이더 컴파일/링크/검증 |
| `diagnostics/uniform_diagnostics.h` | 누락 uniform warn-once / 타입 불일치 |
| `diagnostics/gl_state_log.h` | 현재 GL 상태 한 줄 덤프 + KHR_debug 콜백 |
| `diagnostics/engine_diagnostics.h` | interleaved VBO 검증, 텍스처 채널↔포맷 정합성, 라이팅 uniform 집합, Transform 부모 사이클 가드 |

**주의**: `stb_image` 정의는 *데모 main.cpp* 가 책임 — diagnostics 는 정의 안 함.

---

### 3.3 `SJH::buffer`

#### `Buffer` ([buffer.h](../src/buffer/buffer.h))
```cpp
static BufferUPtr Buffer::CreateWithData(GLuint bufferType, GLuint usage,
                                         const void* data, size_t stride, size_t count);
GLuint  Buffer::Get() const;
bool    Buffer::Bind() const;
```

#### `Framebuffer` ([framebuffer.h](../src/buffer/framebuffer.h))
```cpp
static FramebufferUPtr Framebuffer::Create(int width, int height);          // FBO + color/depth attachment
static FramebufferUPtr Framebuffer::Create(const TexturePtr colorAttachment);
void     Framebuffer::Bind() override;
const TexturePtr Framebuffer::GetColorAttachment() const;
```

**사용 패턴 (multi-pass)**: `Camera::SetTargetFramebuffer(fb)` → RenderSystem 이 `BeginFrame` 시 자동 사용.

---

### 3.4 `SJH::shader`

```cpp
static ShaderUPtr Shader::CreateFromFile(const std::string& filename, GLenum shaderType);
static ShaderUPtr Shader::CreateFromSource(const std::string& source, GLenum shaderType);
GLuint  Shader::GetShaderAddr() const;
```

**컨벤션**: 모든 셰이더 `#version 410 core` (메모리: `glsl_410_project_policy`).

---

### 3.5 `SJH::program`

#### `Program` ([program.h](../src/program/program.h))
```cpp
static ProgramUPtr Program::Create(const std::vector<ShaderPtr>& shaders);
static ProgramUPtr Program::CreateWithVSFS(const std::string& vsFile, const std::string& fsFile);

GLuint              Program::GetProgramAddr() const;
GLint               Program::GetLocation(const char* name) const;   // UniformCache 위임
GLenum              Program::GetType(const char* name) const;
const UniformCache& Program::GetUniformCache() const;

// Observer 패턴 — Material 이 SetProgram(p) 시 자동 호출 (외부 직접 호출 비권장).
void Program::RegisterMaterial(Material*) const;
void Program::UnregisterMaterial(Material*) const;
```

**Observer cascade**: `Program::~Program` 가 등록된 모든 `Material` 의 `OnProgramReleased(this)` 호출 → `Material::mProgram = nullptr`. **dangling 구조적 차단**.

#### `UniformCache` ([uniform_cache.h](../src/program/uniform_cache.h))
```cpp
struct Entry { GLint Location; GLenum Type; };
void   UniformCache::Build(const Program& prog);             // glGetActiveUniform enumerate
GLint  UniformCache::GetLocation(const char* name) const;     // 미존재 -1
GLenum UniformCache::GetType(const char* name) const;
std::size_t UniformCache::Size() const;
const std::unordered_map<std::string, Entry>& UniformCache::Entries() const;   // MaterialApplier outer iteration 용
```

#### `Uniforms::Set*(Program&, ...)` ([program_uniforms.h](../src/program/program_uniforms.h)) — 씬 전역 transient 송신

```cpp
namespace SJH::Uniforms {
    // Basic setter — 즉시 glUniform* 호출. UniformCache 경유 + Diagnostics warn-once.
    void SetMat4 (const Program&, const char* name, const vmath::mat4&);
    void SetVec4 (const Program&, const char* name, const vmath::vec4&);
    void SetVec3 (const Program&, const char* name, const vmath::vec3&);
    void SetVec2 (const Program&, const char* name, const vmath::vec2&);
    void SetFloat(const Program&, const char* name, float);
    void SetInt  (const Program&, const char* name, int);

    // Light struct helper — `<prefix>.{direction,ambient,diffuse,specular,...}` 일괄 송신.
    void SetDirLight  (const Program&, const char* prefix, const DirLight&,   const vmath::vec3& worldDir);
    void SetPointLight(const Program&, const char* prefix, const PointLight&, const vmath::vec3& worldPos);
    void SetSpotLight (const Program&, const char* prefix, const SpotLight&,  const vmath::vec3& worldPos, const vmath::vec3& worldDir);
}
```

**용도** — *Material 의 properties 가 아닌* 송신 경로:
- **per-draw transient** (`uModel`) — `RenderQueue::Flush` 가 DrawCommand 마다 호출.
- **per-pass transient** (`uView` / `uProj` / `viewPos`) — `RenderQueue::Flush` 가 카메라 패스 시작 시 호출.
- **씬 전역 광원** (`dirLight.*` / `pointLights[i].*` / `spotLight.*` / `*Enabled`) — `RenderSystem::SendLightUniforms` 가 모든 program 에 1 회 송신.
- **`MaterialApplier::Apply` 의 내부 구현** — Material properties bag 의 값을 *결국 Program 에 송신* 하는 마지막 단계.

§1 의 *Uniform 송신의 두 layer* 표 참조 — Unity `Shader.SetGlobalFloat` 정통.

---

### 3.6 `SJH::layout`

#### `VertexLayout` ([vertex_layout.h](../src/layout/vertex_layout.h))
```cpp
static VertexLayoutUPtr VertexLayout::Create();
GLuint VertexLayout::GetVAO() const;
bool   VertexLayout::Bind() const;
bool   VertexLayout::TrySetAttrib(GLuint attribIdx, int count, GLuint type,
                                   bool normalized, GLsizei stride, uint64_t offset);
```

**SJH `Vertex` 레이아웃** (모든 Mesh 동일):
- `location 0` : `vec3 aPos`
- `location 1` : `vec3 aNormal`
- `location 2` : `vec2 aTexCoord`

---

### 3.7 `SJH::material`

#### `Material` ([material.h](../src/material/material.h)) — Unity `material.SetXxx` 정통

```cpp
static MaterialUPtr Material::Create();

void Material::SetProgram(const Program* p);    // Observer 등록 + UniformCache reference 보유
const Program*      Material::GetProgram() const;
const UniformCache* Material::GetCache()   const;

MaterialUPtr Material::Clone() const;            // Unity MID / Unreal MID 패턴

// Public properties bag — 직접 접근 가능 (자유함수가 store).
std::unordered_map<std::string, float>           Floats;
std::unordered_map<std::string, int>             Ints;
std::unordered_map<std::string, vmath::vec3>     Vec3s;
std::unordered_map<std::string, vmath::vec4>     Vec4s;
std::unordered_map<std::string, vmath::mat4>     Mat4s;
std::unordered_map<std::string, TextureBinding>  Textures;   // {Tex*, Unit}
```

#### Properties Setter — 자유함수 family ([material_uniforms.h](../src/material/material_uniforms.h))

```cpp
namespace SJH::Uniforms {
    void SetFloat  (Material& mat, const char* name, float v);
    void SetInt    (Material& mat, const char* name, int v);
    void SetVec3   (Material& mat, const char* name, const vmath::vec3& v);
    void SetVec4   (Material& mat, const char* name, const vmath::vec4& v);
    void SetMat4   (Material& mat, const char* name, const vmath::mat4& v);
    void SetTexture(Material& mat, const char* name, const Texture* tex, GLint unit);
}
```

**의미**: properties bag 에 *store 만*. 실제 GL 호출은 `MaterialApplier::Apply` 시점.

#### `MaterialApplier::Apply` ([material_applier.h](../src/render/material_applier.h))

```cpp
void SJH::MaterialApplier::Apply(RenderContext& rc, const Material& mat);
```

**알고리즘** (Cache outer + Type dispatch + Material lookup inner):
1. `cache->Entries()` 순회 — 셰이더 schema 가 진실의 원천.
2. `entry.Type` 으로 어떤 Material typed map 에서 가져올지 분기 (`switch(GL_FLOAT / GL_INT / GL_FLOAT_VEC3 / ... / GL_SAMPLER_2D)`).
3. Material 의 typed map 에 `find` — 없으면 silent skip.

**전제** (호출자 책임): `mat.GetProgram() != nullptr`, `rc.UseProgram(*mat.GetProgram())` 이 이미 호출.

---

### 3.8 `SJH::object`

#### `Transform` ([transform.h](../src/object/transform.h)) — POD-like 값 객체
```cpp
vmath::vec3 Translate = (0,0,0);
vmath::vec3 EulerRot  = (0,0,0);   // degree, X=pitch, Y=yaw, Z=roll, ZYX 합성
vmath::vec3 Scale     = (1,1,1);

vmath::mat4 GetLocalMatrix()    const;   // T · Rz · Ry · Rx · S
vmath::mat4 GetRotationMatrix() const;   // Rz · Ry · Rx (Translate/Scale 무시)

// ── 6 방향 vector (OpenGL 오른손 정통) ──
// EulerRot=(0,0,0) 기본: Right=(+1,0,0), Up=(0,+1,0), Forward=(0,0,-1)
vmath::vec3 GetRight()   const;
vmath::vec3 GetUp()      const;
vmath::vec3 GetForward() const;
vmath::vec3 GetLeft()    const;   // -GetRight
vmath::vec3 GetDown()    const;   // -GetUp
vmath::vec3 GetBack()    const;   // -GetForward
```

#### `Mesh` ([mesh.h](../src/object/mesh.h))
```cpp
static MeshUPtr Mesh::Create(const std::vector<Vertex>& v, const std::vector<GLuint>& i, GLuint primitiveType);
static MeshUPtr Mesh::CreateBox();
static MeshUPtr Mesh::CreatePlane();
static MeshUPtr Mesh::CreateScreenQuad();
GLuint Mesh::GetVAO() const;
GLuint Mesh::GetPrimitiveType() const;
```

#### `Geometry` ([geometry.h](../src/object/geometry.h)) — 정점 데이터 생성기 (Mesh 의 빌더 단계)
- Box / Plane / ScreenQuad / Sphere / Cone / Cylinder 등.

#### Light Components ([light.h](../src/object/light.h)) — Component 상속

```cpp
class DirLight : public Scene::Component {
public:
    vmath::vec3 Ambient, Diffuse, Specular;
    vmath::vec3 GetWorldDirection() const;   // owner.WorldMatrix 의 -Z 컬럼 (OpenGL forward)
};

class PointLight : public Scene::Component {
public:
    float Distance = 32.0f;          // 거리 감쇠 산출 기준
    vmath::vec3 Ambient, Diffuse, Specular;
    vmath::vec3 GetWorldPosition() const;
};

class SpotLight : public Scene::Component {
public:
    float CutoffAngleDeg      = 12.5f;
    float OuterCutoffAngleDeg = 17.5f;
    float Distance            = 32.0f;
    vmath::vec3 Ambient, Diffuse, Specular;
    vmath::vec3 GetWorldPosition()  const;
    vmath::vec3 GetWorldDirection() const;
};

// 단일 점광원 데이터 컨테이너 (Component 아님 — 레거시) — `Light` 클래스.
```

**Actor 가 컴포넌트를 `type_index` 로 저장** — 같은 Actor 에 *PointLight 두 개 부착 불가*. 복수 광원은 Actor 인스턴스를 분리해 구성.

#### `GetAttenuationCoeff(distance)`
PointLight/SpotLight 의 도달 거리 → `(Kc, Kl, Kq)` 거리 감쇠 다항식 도출 (Ogre3D / LearnOpenGL 회귀).

#### `Model` ([model.h](../src/object/model.h)) — Assimp 로드
```cpp
bool LoadByAssimp(const std::string& filename);
const std::vector<RenderUnit>& GetRenderUnits() const;
// RenderUnit = { MeshUPtr mesh; Material* material; }
```

---

### 3.9 `SJH::scene`

#### `Component` ([actor.h](../src/scene/actor.h))
```cpp
class Component {
public:
    virtual ~Component() = default;
    virtual void OnEnter() = 0;
    virtual void OnExit()  = 0;
    virtual void Update(float dt) = 0;
    bool   IsEnabled() const;
    void   SetEnabled(bool e);
    Actor* GetOwner() const;
};
```

#### `Actor` ([actor.h](../src/scene/actor.h))
```cpp
explicit Actor(std::string name = "");

// Tree
Actor* AddChild(std::unique_ptr<Actor> child);   // entered 부모면 child OnEnter() 자동
void   RemoveChild(Actor*);
Actor* GetParent() const;
const std::vector<std::unique_ptr<Actor>>& GetChildren() const;

// Components — Single-Call Contract: ctor + insert + OnEnter 한 호출
template<typename T, typename... Args>
T* AddComponent(Args&&... args);                  // T must derive from Component
template<typename T> T*   GetComponent() const;
template<typename T> void RemoveComponent();
void RemoveAllComponents();

// Identity / Transform / Active / Layer
const std::string& GetName() const;
Transform&         GetTransform();
vmath::mat4        GetWorldMatrix() const;        // 부모 Transform 합성
void               SetLayer(uint32_t);            // Camera::CullingMask 와 AND 검사
uint32_t           GetLayer() const;

// Lifecycle (Cocos 정통)
void OnEnter();
void OnExit();
void Update(float dt);                            // 자기 enabled components → 자식 Actor 재귀
```

**중복 컴포넌트 가드** (assert): `AddComponent<T>` 를 같은 type 으로 두 번 호출 시 디버그 빌드 abort.

#### `Director` ([scene.h](../src/scene/scene.h)) — Cocos `cc::Director` 정통 싱글톤
```cpp
static Director& Director::Get();
Actor&  Director::Root();          // mutable — AddChild 등
void    Director::Enter();
void    Director::Exit();
void    Director::Update(float dt);
void    Director::SetActiveCamera(Camera*);     // Unity Camera.main 정통
Camera* Director::GetActiveCamera() const;
```

#### `Camera` ([camera.h](../src/scene/camera.h)) — **Transform 강제 의존**

```cpp
class Camera : public Component {
public:
    // Public projection — POD-like.
    float FovYDeg = 45.0f, Aspect = 16/9, NearZ = 0.1f, FarZ = 100.0f;
    int      Depth        = 0;       // Unity Camera.depth — 작은 값 먼저
    uint32_t CullingMask  = ~0u;     // Unity Camera.cullingMask

    Camera() = default;
    Camera(float fovY, float aspect, float near, float far);

    vmath::mat4 GetViewMatrix() const;        // owner 미부착 시 assert (Compound Actor 컨벤션)
    vmath::mat4 GetProjectionMatrix() const;

    // TargetLock — Unity Cinemachine Composer / Unreal SpringArm 정통
    void TargetLock(const Actor* target);
    void TargetRelease();
    bool IsLocked() const;
    const Actor* GetLockTarget() const;

    void SetTargetFramebuffer(Framebuffer*);   // multi-pass — nullptr = backbuffer
    Framebuffer* GetTargetFramebuffer() const;
};
```

**중요**: Camera 는 *반드시 Actor 에 부착*. [`Scene::CreateCameraActor()`](../src/scene/compound_actor.h) 가 유일한 정상 생성 경로. owner Transform 의 EulerRot/Translate 가 view 의 진실의 원천. forward = `owner.WorldMatrix` 의 -Z 컬럼.

#### `MeshRenderer` ([components.h](../src/scene/components.h))
```cpp
class MeshRenderer : public Component {
public:
    MeshRenderer() = default;
    MeshRenderer(const Mesh* mesh, const Material* material, int queueLayer = 2000);
    const Mesh*     Mesh;
    const Material* Material;
    bool            Visible    = true;
    int             QueueLayer = 2000;        // Unity: 2000=Opaque, 3000=Transparent
};
```

#### Compound Actor ([compound_actor.h](../src/scene/compound_actor.h)) — free factory

```cpp
namespace SJH::Scene {
    std::unique_ptr<Actor> CreateCameraActor(
        std::string name,
        float fovYDeg = 45.0f, float aspect = 16.0f/9.0f,
        float nearZ = 0.1f, float farZ = 100.0f);

    std::unique_ptr<Actor> CreateDirLightActor(
        std::string name,
        vmath::vec3 direction = (0,-1,0));

    std::unique_ptr<Actor> CreatePointLightActor(
        std::string name,
        vmath::vec3 position,
        float distance = 32.0f);

    std::unique_ptr<Actor> CreateSpotLightActor(
        std::string name,
        vmath::vec3 position,
        vmath::vec3 direction = (0,-1,0),
        float innerCutoffDeg = 12.5f,
        float outerCutoffDeg = 17.5f);
}
```

**direction → EulerRot 변환**: pitch = asin(d.y), yaw = atan2(d.x, -d.z), roll = 0. EulerRot=(0,0,0) 기본 forward = (0,0,-1) 와 일관.

#### `ModelSpawner` ([model_spawner.h](../src/scene/model_spawner.h))
```cpp
namespace SJH::Scene {
    std::vector<Actor*> SpawnEntities(Actor& parent, const Model& model);
}
```
Model 의 RenderUnit 들을 Actor 트리로 펼침.

---

### 3.10 `SJH::render`

#### `RenderSystem` ([render_system.h](../src/render/render_system.h))
```cpp
class RenderSystem {
public:
    void Render();                                                  // 모든 Camera 자동 직렬 렌더
    void Render(const vmath::mat4& viewMat, const vmath::mat4& projMat);   // 단일 view/proj
};
```

**`Render()` 동작 (인자 없는 overload)**:
1. Director 의 모든 Actor 트리 traversal — `Camera` 컴포넌트 수집.
2. `Camera::Depth` 정렬 (작은 값 먼저).
3. 각 Camera 별 `RenderWithCamera`:
   - **viewPos** 도출 — owner.WorldMatrix 의 translate 컬럼.
   - **Light 수집** — Actor 트리 DFS 로 DirLight 첫 1 / PointLight 모두 / SpotLight 첫 1.
   - **Program 수집** — 모든 MeshRenderer.Material.Program 의 unique set.
   - **SendLightUniforms** — 모든 unique program 에 viewPos + Light uniform + enabled int 일괄 송신.
   - **DrawCommand 수집** — `cullingMask & actor.Layer` AND 통과만.
   - **RenderQueue.Flush** — Cache outer Apply → glDraw.

#### `RenderContext` ([render_context.h](../src/render/render_context.h)) — 싱글톤
```cpp
static RenderContext& RenderContext::Get();

void UseProgram(const Program&);                  // SP2 — glUseProgram owner
void BindVAO(GLuint);
void BindTexture(GLuint unit, GLuint tex);
void BindTarget(RenderTarget&);
void Clear(GLbitfield);
void SetDepthTest(bool, GLenum func = GL_LESS);
void SetBlend(bool, GLenum src = GL_SRC_ALPHA, GLenum dst = GL_ONE_MINUS_SRC_ALPHA);
void DrawIndexed(GLsizei count);
void DrawArrays(GLenum mode, GLsizei count);
void BeginFrame(RenderTarget&);                   // BindTarget + Clear + SetDepthTest(true) + SetBlend(true)
void SetDefaultTargetSize(int w, int h);          // 매 프레임 갱신 (창 크기)
```

#### `RenderQueue` + `DrawCommand` ([render_queue.h](../src/render/render_queue.h))
```cpp
struct DrawCommand {
    const Program*  program;
    const Mesh*     mesh;
    const Material* material;
    vmath::mat4     modelMatrix = identity;
    int             queueLayer  = 2000;
    const Actor*    actor;
    float           depth = 0.0f;               // view-space z (back-to-front 정렬)
};

void RenderQueue::Submit(const DrawCommand&);
void RenderQueue::SortMultiStage();              // Layer → Program → Material → Depth
void RenderQueue::Flush(RenderContext&, const vmath::mat4& view, const vmath::mat4& proj);
```

---

### 3.11 `SJH::input`

#### `KeyboardInput<TAction>` ([keyboard_input.h](../src/input/keyboard_input.h)) — 2단 디스패처

```cpp
template<typename TAction>
class KeyboardInput {
public:
    // 1단: 물리 키 ↔ 논리 액션
    void BindKey(TAction action, int glfwKey);
    void UnbindKey(int glfwKey);

    // 2단: 액션 ↔ 핸들러
    void BindHeldHandler   (TAction action, std::function<void()> handler);   // 매 프레임 (held)
    void BindPressHandler  (TAction action, std::function<void()> handler);   // 1회 (key down)
    void BindReleaseHandler(TAction action, std::function<void()> handler);   // 1회 (key up)

    // GLFW 콜백 위임
    void PollHeld (GLFWwindow*);            // 매 프레임 호출 — 누른 키의 held handler 발화
    void Dispatch(int glfwKey, int glfwAction);   // GLFW key callback → press/release
};
```

`TAction` 은 *소비자가 정의* — 입력 모듈은 액션의 의미를 모름 (Cocos `cc.Action` 정신).

#### `MouseInput` ([mouse_input.h](../src/input/mouse_input.h))
```cpp
void BindLookHandler(std::function<void(double dx, double dy)> handler);   // 드래그 중 (dx, dy)
void UnbindLook();
void HandleButton(int button, int action, double x, double y);   // GLFW mouse button 위임
void HandleMove  (double x, double y);                            // GLFW cursor pos 위임
void CancelDrag();
bool IsDragging() const;
```

기본 드래그 버튼 = `GLFW_MOUSE_BUTTON_RIGHT`.

---

### 3.12 `SJH::resource_registry`

#### `ResourceRegistry` ([resource_registry.h](../src/resource_registry/resource_registry.h)) — 싱글톤

```cpp
static ResourceRegistry& Get();

Texture*  CreateTexture (const std::string& key, const Image* image);
Texture*  FindTexture   (const std::string& key);

Material* CreateMaterial(const std::string& key);
Material* FindMaterial  (const std::string& key);

Program*  CreateProgram (const std::string& key,
                         const std::string& vsFile,
                         const std::string& fsFile);
Program*  FindProgram   (const std::string& key);

Mesh*     RegisterMesh  (const std::string& key, MeshUPtr mesh);
Mesh*     FindMesh      (const std::string& key);

Model*    FindModel     (const std::string& key);

void Clear();
```

**책임**: `Texture / Material / Model / Program / Mesh` 의 lifetime owner. 데모 main.cpp 가 모든 자원을 *이 레지스트리에 위탁*. `Find*` 는 non-owning 관찰자 포인터 반환.

#### `Image` ([image.h](../src/resource_registry/image.h))
```cpp
static ImageUPtr Image::Create(const std::string& name, int w, int h, int channels = 4);
static ImageUPtr Image::Load  (const std::string& name, const std::string& filepath);

// Procedural 이미지 (테스트용)
void SetCheckImage(int gridX, int gridY);
void SetWhiteImage();
void SetSingleColorImage(const vmath::vec4& color);
```

**stb_image 정의 책임**: 데모 `main.cpp` 가 *정확히 한 곳에서* `#define STB_IMAGE_IMPLEMENTATION` 후 `#include "stb_image.h"` 해야 link 심볼 생성.

#### `Texture` ([texture.h](../src/resource_registry/texture.h))
```cpp
static TextureUPtr Texture::Create       (int w, int h, uint32_t format);   // 빈 GL 텍스처 (FBO 어태치먼트)
static TextureUPtr Texture::CreateTexture(const Image* image);              // CPU → GPU 업로드

GLuint Texture::GetTextureID() const;
void   Texture::Bind() const;
void   Texture::SetFilter(GLuint min, GLuint mag) const;
void   Texture::SetWrap  (GLuint s, GLuint t) const;
```

---

## 4. 컨벤션

### 4.1 Compound Actor

**Actor 는 *비상속*. 특수 속성은 *Component* 부착으로만 부여.**

새 PreBuilt 추가 시 `compound_actor.h` 에 free factory 추가:
```cpp
std::unique_ptr<Actor> CreatePlayerActor(std::string name, KeyboardInput<X>*, MouseInput*);
```

**금지**:
```cpp
class PlayerActor : public Actor { ... };   // ❌ Actor 서브클래싱 금지
```

(메모리: [`compound_actor_pattern.md`](../.claude/projects/-Users-escatrgot-DevelopProjects-SSU-GlobalMedia-OpenGL-ComputerGraphics/memory/compound_actor_pattern.md))

---

### 4.2 Builder Pattern (Components)

자주 쓰이는 Component 는 *fluent setter + SetUp 마무리* 패턴:
```cpp
auto* ctrl = actor->AddComponent<CameraController>();
ctrl->SetKeyboardInput(&kb)
    .SetMouseInput(&mouse)
    .SetCamera(cam)
    .SetUp();    // 의존 검증 + 등록 — 누락 시 spdlog::error + return false
```

- Setter 는 *멱등* (이미 set 됐으면 no-op) — `SetUp` 한 곳에서만 nullptr 검증.
- `OnExit` 에서 `UnregisterBindings` + 멤버 nullptr 화 (재활성화 가능).

---

### 4.3 Material 셋업

```cpp
// 1. Program 생성 (ResourceRegistry)
auto* prog = reg.CreateProgram("phong", "vs", "fs");

// 2. Material 생성 + Program 주입 (Observer 등록)
auto* mat = reg.CreateMaterial("box");
mat->SetProgram(prog);

// 3. Properties bag 에 store (Unity material.SetFloat 정통)
SJH::Uniforms::SetVec3 (*mat, "material.diffuse",   vmath::vec3(0.8f, 0.3f, 0.3f));
SJH::Uniforms::SetVec3 (*mat, "material.specular",  vmath::vec3(0.5f));
SJH::Uniforms::SetFloat(*mat, "material.shininess", 32.0f);
SJH::Uniforms::SetTexture(*mat, "uMainTex", tex, 0);

// 4. (자동) MaterialApplier::Apply 가 매 프레임 GL 송신
//    셰이더에 없는 properties 는 silent skip — UniformCache 교집합.
```

**Apply 가 송신 안 하는 uniform** (transient state — Material 책임 아님):
- `uModel / uView / uProj` — `RenderQueue::Flush` 가 자동 송신
- `dirLight.* / pointLights[i].* / spotLight.* / viewPos` — `RenderSystem::SendLightUniforms` 가 자동 송신

Material 에는 *셰이더에 정의된 sampler / 사용자 컨텐츠 properties* 만 store. transform/light/viewPos 같은 builtin 은 RenderSystem 이 책임 (Unity `Camera`/`Light` 가 자동 송신하는 builtin uniform 과 동일 정통).

**Clone 패턴** (Unity MID): `auto custom = mat->Clone(); SJH::Uniforms::SetVec3(*custom, "...", ...);` — 공유 템플릿 → per-use 가변.

---

### 4.4 Camera 모드 (Free / TargetLock)

**Free 모드** (기본): owner Transform 의 EulerRot/Translate 가 view 도출.
```cpp
auto camActor = Scene::CreateCameraActor("MainCam", 45.0f, aspect, 0.1f, 100.0f);
// CameraController 가 owner Transform 갱신
```

**TargetLock 모드** (Cinemachine Composer / Unreal SpringArm):
```cpp
auto* cam = camActor->GetComponent<Camera>();
cam->TargetLock(playerActor);   // owner.Translate → playerActor.WorldPos 로 lookat
// → 카메라 위치(owner.Translate)는 유지, 시선만 target 추적.

cam->TargetRelease();   // 자유 시점 복귀 (마지막 EulerRot 으로)
```

Lock 중 CameraController 의 yaw/pitch 갱신은 *상태로만 보존* — 시각 효과는 lock 동안 무효.

---

### 4.5 Uniform 송신의 두 layer — Material 한정 vs 씬 전역

> Unity `material.SetFloat / SetTexture` vs `Shader.SetGlobalFloat / Camera builtin uniform` 분리 정통.

**규칙** — uniform 의 *데이터 소유자* 가 누구인가로 layer 선택:

| 데이터 종류 | layer | 사용 |
|---|---|---|
| Material 자기 컨텐츠 (color/texture/shininess/사용자 properties) | **Material 한정** | `SJH::Uniforms::Set*(Material&, name, value)` — `material_uniforms.h` |
| per-draw transient (`uModel`) | **씬 전역** | `RenderQueue::Flush` 가 자동 — `program_uniforms.h` |
| per-camera-pass transient (`uView` / `uProj` / `viewPos`) | **씬 전역** | `RenderQueue::Flush` + `RenderSystem` 자동 |
| 광원 (DirLight/PointLight/SpotLight + enabled int) | **씬 전역** | `RenderSystem::SendLightUniforms` 자동 — 모든 Program 에 1 회 |
| Apply 내부 송신 | **씬 전역 layer 의 내부 호출** | `MaterialApplier::Apply` 가 `Uniforms::Set*(*prog, ...)` 호출 |

**판정 기준**:
- *DrawCommand 마다 변화하는가* → 씬 전역 (Material 에 store 시 N×Apply 비효율)
- *모든 Material 에 동일하게 적용되는가* → 씬 전역 (Material 에 N개 복사 = SSoT 위반)
- *Material 자기 슬롯의 컨텐츠인가* → Material 한정

**ddd OCP 함의** — 새 셰이더 추가 시:
- Material properties bag 은 *자동 흡수* (셋업 코드만 `SetFloat / SetTexture` 호출 추가).
- builtin transient (transform/light) 는 RenderSystem 이 *이미 모든 program 에 송신* — 추가 작업 0.

### 4.6 Lighting 셰이더 schema

**`RenderSystem::SendLightUniforms`** 가 매 프레임 모든 Program 에 송신하는 uniform:

```glsl
// DirLight
uniform struct { vec3 direction; vec3 ambient; vec3 diffuse; vec3 specular; } dirLight;
uniform int dirLightEnabled;

// PointLight × 2
uniform struct {
    vec3 position; vec3 attenuation;   // (Kc, Kl, Kq)
    vec3 ambient; vec3 diffuse; vec3 specular;
} pointLights[2];
uniform int pointLightsEnabled[2];

// SpotLight × 1
uniform struct {
    vec3 position; vec3 direction;
    float cutoff;      // cos(inner)
    float outerCutoff; // cos(outer)
    vec3 attenuation;
    vec3 ambient; vec3 diffuse; vec3 specular;
} spotLight;
uniform int spotLightEnabled;

// 카메라
uniform vec3 viewPos;

// 변환
uniform mat4 uModel, uView, uProj;
```

셰이더 schema 가 cache 에 없는 uniform 은 송신 안 됨 — *silent skip*. 셰이더가 raster 만 하면 light 무시 가능.

**Light 부족 슬롯**: enabled = 0 으로 강제 (RenderSystem 이 자동).

---

## 5. 빠른 시작 — 새 챕터

`apps/<chapter>/` 디렉토리에:

1. **`main.cpp`** — sb7 application 상속:
```cpp
#include <sb7.h>   // 반드시 *맨 앞* (gl3w + glcorearb typedef 순서)
#include "scene/compound_actor.h"
#include "render/render_system.h"
// ...

class my_app : public sb7::application {
    void init() override {
        sb7::application::init();
        info.majorVersion = 4; info.minorVersion = 1;   // GLSL 410 정통
    }
    void startup() override { /* Actor/Material 셋업 */ }
    void render(double t) override { Director::Get().Update(dt); mRenderSys.Render(); }
    void shutdown() override { Director::Get().Exit(); }
    void onResize(int w, int h) override { /* glViewport + Camera.Aspect 갱신 */ }
    SJH::RenderSystem mRenderSys;
};
DECLARE_MAIN(my_app);
```

2. **`CMakeLists.txt`**:
```cmake
get_filename_component(CHAPTER_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
add_executable(${CHAPTER_NAME} main.cpp)
target_link_libraries(${CHAPTER_NAME} PRIVATE project_deps game_deps SJH::engine)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources)
    add_custom_command(TARGET ${CHAPTER_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${CHAPTER_NAME}>/resources)
endif()
```

3. **`apps/CMakeLists.txt`** 에 `add_subdirectory(<chapter>)` 추가.

4. **`resources/shaders/*.vs / *.fs`** — `#version 410 core` 필수.

빌드:
```bash
cmake --build --preset ninja --target <chapter>
cd build_ninja/apps/<chapter> && ./<chapter>
```

---

## 6. 빌드 시스템

### Dependency layer (`cmake/Dependency.cmake`)

| INTERFACE 타겟 | 포함 | 사용 시점 |
|---|---|---|
| `project_deps` | sb7 + glfw3 + OpenGL + 플랫폼 프레임워크 (Cocoa/Win32) | *모든 챕터* |
| `game_deps` | Box2D + Effekseer + assimp + spdlog + Tweeny + stb + (조건부 FMOD) | 게임/엔진 챕터만 |
| `SJH::engine` | **12 코어 모듈 우산** | *모든 챕터* |

```cmake
target_link_libraries(my_chapter PRIVATE project_deps SJH::engine)            # 일반 챕터
target_link_libraries(my_chapter PRIVATE project_deps game_deps SJH::engine)  # 게임 챕터
```

### 모듈 의존 선언 컨벤션

- 헤더에 노출되는 의존 → `PUBLIC`
- `.cpp` 내부 전용 → `PRIVATE`
- INTERFACE 모듈 (`SJH::material`) → `INTERFACE`

### 빌드 명령 (macOS/Linux)

```bash
cmake --preset ninja                                # Debug configure
cmake --preset ninja -DENABLE_TESTING=ON            # 테스트 빌드 활성화
cmake --build --preset ninja --target <chapter>     # 단일 챕터 빌드
ctest --test-dir build_ninja -V                     # 테스트 실행 (ENABLE_TESTING 필요)
./build_ninja/test/test_material                    # 직접 실행
```

### Windows (MSVC)

```bash
cmake --preset msvc-2022 -A x64                                # ARM64 호스트에서 x64 강제
cmake --build --preset msvc-2022 --target <chapter>
```

---

## 부록: 변경 이력 (Sprint 단위)

| Sprint | 핵심 변경 |
|---|---|
| SP1 | Shader/Program 리소스 통합 |
| SP2 | RenderContext — `glUseProgram` 단일 owner |
| SP3 | Actor + Component + Scene Graph + RenderSystem + RenderQueue |
| SP3.5 | Director::ActiveCamera + Camera::InverseAffine + Aspect setter |
| SP4 | Multi-pass — Camera::TargetFramebuffer + Depth + CullingMask + Layer / postfx_demo |
| SP5 | Light Component 화 (DirLight/PointLight/SpotLight) + RenderSystem 의 light 수집/송신 |
| SP6 | UniformCache 분리 + Program Observer 패턴 + Material properties bag + Uniforms 자유함수 family + MaterialApplier 알고리즘 단일화 (Cache outer + Type dispatch + Material lookup) |
| SP7 | KeyboardInput<T> + MouseInput + CameraController (Transform-based + Builder Pattern) |
| SP8 | Compound Actor 컨벤션 (`compound_actor.h` free factory) + Camera Transform 강제 의존 + TargetLock (Cinemachine Composer 정통) + Transform 6 방향 vector + Light 컨벤션 통일 (-Z forward) + migrate_demo 통합 챕터 |

---

## 부록: 메모리 (`.claude/.../memory/`) 핵심 컨벤션

| 메모리 | 요약 |
|---|---|
| `glsl_410_project_policy` | 모든 셰이더 `#version 410 core` 강제 |
| `sb7code_immutable` | `extern/sb7code` 절대 수정 금지 |
| `vmath_radians_int_trap` | `radians(120)` 같은 정수 리터럴은 T=int 추론 0. `120.0f` 사용 |
| `entt_removed` | entt 사용 폐기 (OOP Actor + Component 선호) |
| `fmod_game_deps_auto_join` | FMOD 는 `game_deps` 자동 합류. dynamic dll 은 챕터 POST_BUILD copy |
| `compound_actor_pattern` | PreBuilt = `compound_actor.h` free factory. Actor 비상속 |
| `pessimizing_move_intentional` | `return std::move(local)` 은 의도된 패턴. 자발 fix 금지 |

---

**문서 끝**. 누락된 API / 컨벤션 발견 시 본 문서 갱신 + 메모리 동기화.
