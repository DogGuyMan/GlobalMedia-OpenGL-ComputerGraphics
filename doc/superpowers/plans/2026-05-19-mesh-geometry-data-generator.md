# Mesh ↔ Geometry 데이터 생성기 분리 — Implementation Plan

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** mesh.cpp 의 하드코딩 정점 데이터를 제거하고, 도형 데이터 생성 책임을 engine 빌더에 위임하는 어댑터 `SJH::Geometry` 로 옮긴다.

**Architecture:** 신설 `SJH::Geometry` (src/object/geometry.{h,cpp}) 가 `Engine::Model::BuildXxxIndexed` 를 호출해 13-float interleaved 출력을 `SJH::Vertex` 로 변환한 `MeshData` 를 반환한다. `Mesh::CreateBox`/`CreatePlane` 은 이 데이터를 소비만 한다. GL 로더 충돌(gl3w vs glad)은 번역 단위 격리로 해결 — `Vertex` 를 로더 비의존 헤더로 분리한다.

**Tech Stack:** C++17, CMake (Ninja preset), vmath (sb7), 기존 `SJH::diagnostics`/`SJH::buffer`/`SJH::layout` 모듈.

**참고 스펙:** `doc/superpowers/specs/2026-05-19-mesh-geometry-data-generator-design.md`

**테스트 정책:** 이 프로젝트는 단위 테스트 프레임워크가 없다 (CLAUDE.md). 각 태스크의 검증은 **`SJH::object` 정적 라이브러리(`sjhopengl_object`) 컴파일 성공** 으로 한다. 컴파일이 곧 회귀 게이트다.

---

### Task 1: `Vertex` 구조체를 로더 비의존 헤더로 분리

`struct Vertex` 는 `vmath` 만 필요하고 GL 로더가 불필요하다. 현재 `mesh.h` 안에 있어 glad 를 함께 끌어온다. 별도 헤더로 빼면 gl3w 세계(geometry.cpp)에서도 안전하게 include 할 수 있다.

**Files:**
- Create: `src/object/vertex.h`
- Modify: `src/object/mesh.h:24-34`

- [ ] **Step 1: `src/object/vertex.h` 생성**

```cpp
#ifndef __OBJECT_VERTEX_H__
#define __OBJECT_VERTEX_H__

#include <vmath.h>

namespace SJH
{
    /// @brief 단일 정점 — 위치 + 법선 + UV 좌표.
    /// @note GL 로더(glad/gl3w) 비의존 — vmath 만 의존하므로 어느 TU 에서도 안전.
    struct Vertex
    {
        vmath::vec3 position; ///< 정점 위치 (object space)
        vmath::vec3 normal;   ///< 법선 벡터 (object space, 정규화 가정)
        vmath::vec2 texCoord; ///< UV 좌표 (0~1 범위 권장)
    };
}

#endif // __OBJECT_VERTEX_H__
```

- [ ] **Step 2: `src/object/mesh.h` 에서 `struct Vertex` 제거 + vertex.h include**

`mesh.h` 의 include 블록과 `struct Vertex` 정의(현재 24~34행)를 찾아 교체한다.

찾을 내용:
```cpp
#include "buffer/buffer.h"
#include "common/common.h"
#include "layout/vertex_layout.h"
#include <<glad>/glad.h>
#include <vmath.h>

namespace SJH
{
    /// @brief 단일 정점 — 위치 + 법선 + UV 좌표.
    struct Vertex
    {
        vmath::vec3 position; ///< 정점 위치 (object space)
        vmath::vec3 normal;   ///< 법선 벡터 (object space, 정규화 가정)
        vmath::vec2 texCoord; ///< UV 좌표 (0~1 범위 권장)
    };

    CLASS_PTR(Mesh);
```

교체 내용:
```cpp
#include "buffer/buffer.h"
#include "common/common.h"
#include "layout/vertex_layout.h"
#include "object/vertex.h"
#include <<glad>/glad.h>

namespace SJH
{
    CLASS_PTR(Mesh);
```

(`<vmath.h>` 직접 include 제거 — `Vertex` 가 빠지면서 mesh.h 본문은 vmath 타입을 직접 쓰지 않는다. `vertex.h` 가 transitive 로 제공. `<<glad>/glad.h>` 는 메서드 시그니처의 `GLuint` 때문에 유지.)

- [ ] **Step 3: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target sjhopengl_object`
Expected: 빌드 성공. `mesh.cpp` 가 `mesh.h` 경유로 `Vertex` 를 그대로 인식 (정의 위치만 이동).

- [ ] **Step 4: 커밋**

```bash
git add src/object/vertex.h src/object/mesh.h
git commit -m "$(cat <<'EOF'
[refactor] : Vertex 구조체를 vertex.h 로 분리 (GL 로더 비의존)

mesh.h 안의 struct Vertex 를 별도 헤더로 추출.
gl3w 세계(향후 geometry.cpp)에서도 glad 충돌 없이 include 가능.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: `src/object/geometry.cpp` 를 object 모듈 빌드에 편입

`SJH::Geometry` 가 위임할 `Engine::Model::BuildXxxIndexed` 의 정의는 `src/object/geometry.cpp` 에 있으나, `src/CMakeLists.txt` 에 `add_subdirectory(engine)` 가 없어 현재 빌드되지 않는다. 이 한 파일만 object 모듈에서 in-place 컴파일한다. (engine 의 다른 5개 소스는 건드리지 않음 — 컴파일 리스크 최소화.)

**Files:**
- Modify: `src/object/CMakeLists.txt:1`

- [ ] **Step 1: `src/object/CMakeLists.txt` 의 `add_library` 소스 목록에 src/object/geometry.cpp 추가**

찾을 내용:
```cmake
add_library(sjhopengl_object STATIC mesh.cpp model.cpp scene_node.cpp)
```

교체 내용:
```cmake
add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    scene_node.cpp
    # SJH::Geometry 가 위임하는 Engine::Model 빌더 정의.
    # engine 모듈 전체를 빌드하지 않고 이 파일만 in-place 컴파일.
    # 주의: 훗날 src/CMakeLists.txt 에 add_subdirectory(engine) 가 켜지면
    #       중복 컴파일(중복 심볼) 발생 — 그 시점에 이 줄을 제거할 것.
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/geometry.cpp
)
```

(include 경로 변경 불필요 — object 모듈은 이미 PUBLIC 으로 `src/`(`..`) 와 `${CMAKE_SOURCE_DIR}/include` 를 가진다. `src/object/geometry.cpp` 가 include 하는 `src/object/geometry.h`/`<engine>/constants.h` 및 그 하위 `GL/gl3w.h`/`GL/glcorearb.h`/`vmath.h` 모두 해석된다.)

- [ ] **Step 2: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target sjhopengl_object`
Expected: 빌드 성공. `src/object/geometry.cpp` 가 이 빌드에서 처음 컴파일되며 통과해야 한다. `src/object/geometry.cpp` 는 GL *함수* 를 호출하지 않으므로(타입만 사용) 링크 심볼 충돌 없음.

> 만약 `src/object/geometry.cpp` 자체 컴파일 에러가 드러나면(휴면 코드), 해당 에러를 그 자리에서 수정한다. `<cmath>`/`M_PI` 는 `<engine>/constants.h` 의 `#include <cmath>` 와 전역 `_USE_MATH_DEFINES`(cmake/CXXStandard.cmake)로 이미 제공된다.

- [ ] **Step 3: 커밋**

```bash
git add src/object/CMakeLists.txt
git commit -m "$(cat <<'EOF'
[build] : src/object/geometry.cpp 를 object 모듈 빌드에 편입

SJH::Geometry 위임 대상인 Engine::Model 빌더를 컴파일하기 위해
src/object/geometry.cpp 단일 파일만 sjhopengl_object 소스에 추가.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: `SJH::Geometry` 어댑터 모듈 생성

`MeshData` 타입과 8개 도형 함수를 선언/정의한다. 각 함수는 `Engine::Model::BuildXxxIndexed` 를 호출하고 `FromInterleaved` 로 `MeshData` 변환한다.

**Files:**
- Create: `src/object/geometry.h`
- Create: `src/object/geometry.cpp`
- Modify: `src/object/CMakeLists.txt`

- [ ] **Step 1: `src/object/geometry.h` 생성**

```cpp
#ifndef __OBJECT_GEOMETRY_H__
#define __OBJECT_GEOMETRY_H__

#include "object/vertex.h"
#include <cstdint>
#include <vector>

namespace SJH
{
    /// @brief Geometry 생성기의 출력 — Mesh::Create 가 그대로 소비하는 정점/인덱스 쌍.
    struct MeshData
    {
        std::vector<Vertex>   vertices; ///< 정점 배열 (position + normal + texCoord)
        std::vector<uint32_t> indices;  ///< 인덱스 배열 (uint32_t ≡ GLuint)
    };

    /// @brief engine 빌더에 위임하는 도형 데이터 생성기. 출력은 object-space 정규 형상.
    /// @note 배치(offset)는 노출하지 않음 — Transform 책임. winding 반전만 back_face 로 제어.
    namespace Geometry
    {
        /// @brief 원점 중심 단위 큐브 (24정점 / 36인덱스).
        MeshData Box(bool back_face = false);
        /// @brief 원점 중심 단위 XY quad, z=0 (4정점 / 6인덱스).
        MeshData Plane(bool back_face = false);
        /// @brief 사각뿔 — 4 옆면 + 바닥 quad.
        MeshData Cone(bool back_face = false);
        /// @brief 정사면체.
        MeshData Tetrahedron(bool back_face = false);
        /// @brief 정팔면체.
        MeshData Octahedron(bool back_face = false);

        /// @brief 원판/고리 파라메트릭 서피스 (XZ 평면).
        /// @param us,ue,uRes 각도 범위[rad]와 분할 수. @param vs,ve,vRes 반지름 비율(0~1)과 분할.
        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius = 1.0f, bool back_face = false);
        /// @brief 원기둥 옆면.
        /// @param us,ue,uRes 원주 각도와 분할. @param vs,ve,vRes 높이 비율과 분할.
        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius = 1.0f, float height = 1.0f,
                          bool back_face = false);
        /// @brief 반구 (북반구, +Y).
        /// @param us,ue,uRes 경도와 분할. @param vs,ve,vRes 위도 비율(0~1→PI/2)과 분할.
        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius = 1.0f, bool back_face = false);
    } // namespace Geometry
} // namespace SJH

#endif // __OBJECT_GEOMETRY_H__
```

- [ ] **Step 2: `src/object/geometry.cpp` 생성**

```cpp
#include "object/geometry.h"
#include "<engine>/constants.h"
#include "src/object/geometry.h"

namespace SJH
{
    namespace
    {
        // engine 의 13-float interleaved(pos4 + color4 + normal3 + uv2) 출력을
        // SJH::Vertex(pos3 + normal3 + uv2) 로 변환. color(4 float)는 폐기.
        MeshData FromInterleaved(const std::vector<GLfloat> &raw,
                                 const std::vector<GLuint> &idx)
        {
            constexpr int STRIDE = Engine::Constants::GEOMETRY::VERTEX_LEN; // 13
            MeshData data;
            const size_t count = raw.size() / STRIDE;
            data.vertices.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                const size_t b = i * STRIDE;
                Vertex v;
                v.position = vmath::vec3(raw[b + 0], raw[b + 1], raw[b + 2]); // pos.w 폐기
                v.normal = vmath::vec3(raw[b + 8], raw[b + 9], raw[b + 10]);
                v.texCoord = vmath::vec2(raw[b + 11], raw[b + 12]);
                data.vertices.push_back(v);
            }
            data.indices.reserve(idx.size());
            for (GLuint k : idx)
                data.indices.push_back(static_cast<uint32_t>(k));
            return data;
        }
    } // namespace

    namespace Geometry
    {
        MeshData Box(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildCubeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Plane(bool back_face)
        {
            using namespace Engine::Constants::GEOMETRY;
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            // engine 에 BuildPlaneIndexed 가 없어 저수준 BuildQuadIndexed 를
            // QUAD_BASE_POSITION(z=0 XY quad)으로 직접 호출 — Cone 바닥 quad 와 동일 패턴.
            Engine::Model::BuildQuadIndexed(
                raw, idx,
                QUAD_BASE_POSITION, COLOR_ALL_WHITE_6, QUAD_BASE_MESH_UVS,
                QUAD_MESH_UVS_FAN,
                vmath::vec3(-0.5f, -0.5f, 0.0f),
                back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES);
            return FromInterleaved(raw, idx);
        }

        MeshData Cone(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildConeIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Tetrahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildTetrahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Octahedron(bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildOctahedronIndexed(
                raw, idx, vmath::vec3(-0.5f, -0.5f, -0.5f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildDiskIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius, float height, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildCylinderIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, height, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }

        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius, bool back_face)
        {
            std::vector<GLfloat> raw;
            std::vector<GLuint> idx;
            Engine::Model::BuildHemiSphereIndexed(
                raw, idx, us, ue, uRes, vs, ve, vRes,
                radius, vmath::vec3(0.0f, 0.0f, 0.0f), back_face);
            return FromInterleaved(raw, idx);
        }
    } // namespace Geometry
} // namespace SJH
```

- [ ] **Step 3: `src/object/CMakeLists.txt` 소스 목록에 geometry.cpp 추가**

찾을 내용:
```cmake
add_library(sjhopengl_object STATIC
    mesh.cpp
    model.cpp
    scene_node.cpp
    # SJH::Geometry 가 위임하는 Engine::Model 빌더 정의.
    # engine 모듈 전체를 빌드하지 않고 이 파일만 in-place 컴파일.
    # 주의: 훗날 src/CMakeLists.txt 에 add_subdirectory(engine) 가 켜지면
    #       중복 컴파일(중복 심볼) 발생 — 그 시점에 이 줄을 제거할 것.
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/geometry.cpp
)
```

교체 내용:
```cmake
add_library(sjhopengl_object STATIC
    mesh.cpp
    geometry.cpp
    model.cpp
    scene_node.cpp
    # SJH::Geometry 가 위임하는 Engine::Model 빌더 정의.
    # engine 모듈 전체를 빌드하지 않고 이 파일만 in-place 컴파일.
    # 주의: 훗날 src/CMakeLists.txt 에 add_subdirectory(engine) 가 켜지면
    #       중복 컴파일(중복 심볼) 발생 — 그 시점에 이 줄을 제거할 것.
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/geometry.cpp
)
```

- [ ] **Step 4: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target sjhopengl_object`
Expected: 빌드 성공. `geometry.cpp` 가 gl3w(src/object/geometry.h 경유)만 보는 TU 로 컴파일됨 — glad 와 섞이지 않는다. 아직 호출처가 없어 동작 변화 없음.

- [ ] **Step 5: 커밋**

```bash
git add src/object/geometry.h src/object/geometry.cpp src/object/CMakeLists.txt
git commit -m "$(cat <<'EOF'
[feat] : SJH::Geometry 도형 데이터 생성기 어댑터 추가

MeshData + 8개 도형 함수(Box/Plane/Cone/Tetrahedron/Octahedron/
Disk/Cylinder/HemiSphere). Engine::Model 빌더에 위임하고
13-float interleaved 출력을 SJH::Vertex 로 변환(color 폐기).

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: `Mesh::CreateBox` / `CreatePlane` 를 Geometry 위임으로 전환

mesh.cpp 의 하드코딩 정점/인덱스 리터럴을 전부 제거하고 `SJH::Geometry` 출력을 소비한다.

**Files:**
- Modify: `src/object/mesh.cpp:1-2` (include 추가)
- Modify: `src/object/mesh.cpp:34-100` (`CreateBox` + `CreatePlane` 본문 교체)

- [ ] **Step 1: `mesh.cpp` 상단에 geometry.h include 추가**

찾을 내용:
```cpp
#include "mesh.h"
#include "diagnostics/gl_validate.h" // Cat A — CheckIndices
```

교체 내용:
```cpp
#include "mesh.h"
#include "object/geometry.h"
#include "diagnostics/gl_validate.h" // Cat A — CheckIndices
```

- [ ] **Step 2: `CreateBox` 와 `CreatePlane` 본문을 위임 코드로 교체**

찾을 내용 — `CreateBox()` 의 `{` 부터 `CreatePlane()` 의 닫는 `}` 까지 (현재 35~100행, `clang-format off/on` 블록과 정점/인덱스 리터럴 전체 포함):

```cpp
    MeshUPtr Mesh::CreateBox()
    {
        // clang-format off
        std::vector<Vertex> vertices = {
```
… (24개 Vertex 리터럴 + 36개 index 리터럴) …
```cpp
        return Create(vertices, indices, GL_TRIANGLES);
    }
```

즉 `MeshUPtr Mesh::CreateBox()` 선언 줄부터 `MeshUPtr Mesh::CreatePlane()` 본문의 마지막 `}` 까지 두 함수 전체를 아래로 교체한다:

```cpp
    MeshUPtr Mesh::CreateBox()
    {
        // 도형 데이터 생성은 SJH::Geometry 책임 — engine 빌더에 위임.
        MeshData data = Geometry::Box();
        return Create(data.vertices, data.indices, GL_TRIANGLES);
    }

    MeshUPtr Mesh::CreatePlane()
    {
        MeshData data = Geometry::Plane();
        return Create(data.vertices, data.indices, GL_TRIANGLES);
    }
```

(`Create` 시그니처는 그대로. `data.indices` 는 `std::vector<uint32_t>` 이고 `Create` 는 `std::vector<GLuint>` 를 받으나 `uint32_t ≡ GLuint` 라 동일 타입 — mesh.cpp 의 기존 `Create`/`Init` 정의가 이미 `uint32_t` 를 쓰며 이 등가에 의존한다.)

- [ ] **Step 3: 빌드 검증**

Run: `cmake --preset ninja && cmake --build --preset ninja --target sjhopengl_object`
Expected: 빌드 성공. `mesh.cpp` TU 는 glad 만(geometry.h 는 vertex.h 만 끌어와 로더 비의존) — gl3w 와 섞이지 않는다.

- [ ] **Step 4: (선택) 시각 검증**

`SJH::object` 를 링크하고 `Mesh::CreateBox()` / `CreatePlane()` 을 호출하는 활성 챕터가 있다면 빌드·실행해 큐브·평면이 기존과 동일하게 렌더링되는지 육안 확인한다.
Run 예: `cmake --build --preset ninja --target <chapter>` 후 `cd build_ninja/apps/<chapter> && ./<chapter>`
Expected: 정육면체·평면이 정상 렌더. (normal/UV 가 engine 방식으로 재도출되어 미세 차이는 있을 수 있으나 형상은 동일 — 스펙 §9.)

- [ ] **Step 5: 커밋**

```bash
git add src/object/mesh.cpp
git commit -m "$(cat <<'EOF'
[refactor] : Mesh::CreateBox/CreatePlane 를 Geometry 위임으로 전환

하드코딩 정점/인덱스 리터럴 제거 — 데이터 생성은 SJH::Geometry,
Mesh 는 생성된 MeshData 를 소비만 한다.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

**1. Spec coverage**

| 스펙 항목 | 구현 태스크 |
|-----------|-------------|
| §3 `Vertex` 를 로더 비의존 헤더로 분리 | Task 1 |
| §3 GL 로더 TU 격리 | Task 1(vertex.h) + Task 3(geometry.cpp gl3w) + Task 4(mesh.cpp glad) |
| §4 `vertex.h` 신설 | Task 1 |
| §4·§5 `geometry.h` (MeshData + 8 함수) | Task 3 Step 1 |
| §4·§6 `geometry.cpp` (FromInterleaved + 8 위임) | Task 3 Step 2 |
| §4·§7 `mesh.h`/`mesh.cpp` 수정 | Task 1 Step 2, Task 4 |
| §8 빌드 배선 (src/object/geometry.cpp + geometry.cpp) | Task 2, Task 3 Step 3 |
| §9 double-compile 주석 명시 | Task 2 Step 1 (CMake 주석) |
| §10 검증 (빌드 + 시각) | 각 태스크 빌드 검증 + Task 4 Step 4 |

누락 없음.

**2. Placeholder scan**

"TBD"/"TODO"/"적절히 처리"/"비슷하게" 없음. 모든 코드 스텝에 완전한 코드 블록 포함. ✓

**3. Type consistency**

- `MeshData { std::vector<Vertex> vertices; std::vector<uint32_t> indices; }` — Task 3 정의, Task 4 에서 `data.vertices`/`data.indices` 로 동일하게 사용. ✓
- `Geometry::Box(bool)` / `Geometry::Plane(bool)` — Task 3 선언/정의, Task 4 에서 인자 없이 호출(기본값 `false`). ✓
- `FromInterleaved(const std::vector<GLfloat>&, const std::vector<GLuint>&)` — Task 3 내부 전용, 8개 함수 모두 동일 시그니처로 호출. ✓
- `Vertex { position, normal, texCoord }` — Task 1 정의, Task 3 `FromInterleaved` 에서 동일 필드명 사용. ✓
