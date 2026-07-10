# Mesh ↔ Geometry 데이터 생성기 분리 설계

> ⚠ 2026-05 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

날짜: 2026-05-19
대상: `src/object/mesh.{h,cpp}`, 신설 `src/object/{vertex,geometry}.{h,cpp}`, 기존 `src/engine/geometry.{h,cpp}`

## 1. 동기

`Mesh::CreateBox` / `Mesh::CreatePlane` 은 정점·인덱스 리터럴을 함수 본문에
하드코딩하고 있다. 메시 *데이터 생성* 과 *GPU 리소스 소유* 라는 두 책임이
한 파일에 섞여 있다.

`src/object/geometry.cpp` 에는 이미 검증된 도형 생성기
(`BuildCubeIndexed` / `BuildConeIndexed` / `BuildDiskIndexed` 등)가 존재하지만,
`Mesh` 와 정점 표현·수학 라이브러리 경로가 달라 직접 소비하지 못한다.

목표:
- mesh.cpp 의 직접 데이터 생성 코드를 전부 제거한다.
- 데이터 생성 책임을 단일 모듈 `SJH::Geometry` 로 모은다.
- `Mesh` 는 `SJH::Geometry` 가 만든 정점/인덱스 벡터를 *소비* 만 한다.
- `src/object/geometry.cpp` 의 도형 생성 역할 전체를 `SJH::Geometry` 로 옮긴다
  (단 engine 측 파일은 보존 — `model_base` / `shader_program` 가 계속 참조).

## 2. 접근법

### 채택 — engine 빌더 위임 어댑터

`SJH::Geometry` 는 shape 수학을 재구현하지 않고 `Engine::Model::BuildXxxIndexed`
를 호출한 뒤 `GLfloat[13]` interleaved 출력을 `SJH::Vertex` 로 변환한다.
도형 수식의 단일 출처(engine)를 유지한다.

### 기각 — shape 수학 재구현

`SJH::Geometry` 가 도형 수식을 직접 구현. engine 의존이 사라지지만 검증된
수식이 두 곳으로 갈라진다. 사용자가 "engine 직접 소비" 를 선택해 기각.

## 3. GL 로더 격리 (핵심 제약)

- `src/object/geometry.h` → `GL/gl3w.h` 를 transitive include.
- `mesh.h` → `<glad>/glad.h` 를 include.
- gl3w 와 glad 를 한 TU 에서 섞으면 GL 심볼 재정의 충돌.

해결 — 두 로더가 한 번역 단위에서 절대 만나지 않게 한다:

| 번역 단위 | include 하는 GL 로더 |
|-----------|----------------------|
| `mesh.cpp` | glad 만 |
| `object/geometry.cpp` | gl3w 만 (src/object/geometry.h 경유) |
| `src/object/geometry.cpp` | gl3w 만 |

두 세계는 순수 `vmath` 타입(`Vertex` / `MeshData`)으로만 통신한다.
이를 위해 `struct Vertex` 를 GL 로더에 의존하지 않는 별도 헤더
`object/vertex.h` 로 분리한다 (현재는 glad 를 끌어오는 `mesh.h` 안에 있음).

`src/object/geometry.cpp` 는 GL *함수* 를 호출하지 않고 타입(`GLfloat`/`GLuint`)만
쓰므로, 같은 정적 라이브러리에 링크돼도 GL 함수 포인터 심볼 충돌이 없다.

## 4. 파일 구성

| 파일 | 상태 | 역할 |
|------|------|------|
| `src/object/vertex.h` | 신설 | `struct SJH::Vertex` 를 mesh.h 에서 분리. `<vmath.h>` 만 의존 — GL 로더 미포함 |
| `src/object/geometry.h` | 신설 | `struct MeshData` + `namespace SJH::Geometry` 도형 함수 8종. `vertex.h` 만 의존 |
| `src/object/geometry.cpp` | 신설 | `Engine::Model::BuildXxxIndexed` 위임 + `GLfloat[13]` → `Vertex` 변환 |
| `src/object/mesh.h` | 수정 | `struct Vertex` 제거 → `#include "vertex.h"` |
| `src/object/mesh.cpp` | 수정 | `CreateBox`/`CreatePlane` 하드코딩 제거 → `Geometry::Box()`/`Plane()` 위임 |
| `src/object/CMakeLists.txt` | 수정 | `geometry.cpp` + `../engine/geometry.cpp` 를 소스에 추가 |
| `src/engine/geometry.{h,cpp}` | 불변 | 보존 — `model_base` / `shader_program` 가 계속 참조 |

## 5. API / 타입 (`src/object/geometry.h`)

```cpp
#ifndef __OBJECT_GEOMETRY_H__
#define __OBJECT_GEOMETRY_H__
#include "object/vertex.h"
#include <vector>
#include <cstdint>

namespace SJH
{
    struct MeshData
    {
        std::vector<Vertex>   vertices;
        std::vector<uint32_t> indices;
    };

    // engine 빌더에 위임하는 도형 데이터 생성기. 출력은 object-space 정규 형상.
    // 배치(offset)는 노출 안 함 — Transform 책임. winding 반전만 back_face 로 제어.
    namespace Geometry
    {
        MeshData Box(bool back_face = false);
        MeshData Plane(bool back_face = false);
        MeshData Cone(bool back_face = false);
        MeshData Tetrahedron(bool back_face = false);
        MeshData Octahedron(bool back_face = false);

        MeshData Disk(double us, double ue, int uRes,
                      double vs, double ve, int vRes,
                      float radius = 1.0f, bool back_face = false);
        MeshData Cylinder(double us, double ue, int uRes,
                          double vs, double ve, int vRes,
                          float radius = 1.0f, float height = 1.0f,
                          bool back_face = false);
        MeshData HemiSphere(double us, double ue, int uRes,
                            double vs, double ve, int vRes,
                            float radius = 1.0f, bool back_face = false);
    }
}
#endif
```

설계 결정:
- **`Cube` 없음** — `Box()` 가 곧 engine 의 cube. `Mesh::CreateBox` 명명과 일치시켜 `Box` 채택.
- **비인덱스 빌더(`BuildXxx`) 미노출** — Mesh 는 EBO 필수라 `BuildXxxIndexed` 만 의미 있음.
  비인덱스 경로 · `ComputeFaceNormal` · `PushVertex` 는 engine 내부에 잔류.
- **`offset` 미노출** — object-space 는 정규 원점, 배치는 `transform.h` 책임.
- `uint32_t` ≡ `GLuint` (둘 다 32bit unsigned) — geometry.h 가 GL 헤더를
  안 끌어오게 `<cstdint>` 사용. mesh.cpp 의 기존 `uint32_t` 사용과 일관.

## 6. 데이터 흐름 / 변환 (`src/object/geometry.cpp`)

```
SJH::Geometry::Box()        → Engine::Model::BuildCubeIndexed(raw, idx, ...)
SJH::Geometry::Plane()      → Engine::Model::BuildQuadIndexed(raw, idx,
                                  QUAD_BASE_POSITION, COLOR_ALL_WHITE_6,
                                  QUAD_BASE_MESH_UVS, QUAD_MESH_UVS_FAN, ...)
SJH::Geometry::Cone()       → Engine::Model::BuildConeIndexed(...)
SJH::Geometry::Tetrahedron()→ Engine::Model::BuildTetrahedronIndexed(...)
SJH::Geometry::Octahedron() → Engine::Model::BuildOctahedronIndexed(...)
SJH::Geometry::Disk(...)    → Engine::Model::BuildDiskIndexed(...)
SJH::Geometry::Cylinder(...)→ Engine::Model::BuildCylinderIndexed(...)
SJH::Geometry::HemiSphere(...)→ Engine::Model::BuildHemiSphereIndexed(...)

공통 후처리: raw(vector<GLfloat>, 13/정점) + idx(vector<GLuint>)
             → FromInterleaved(raw, idx) → MeshData
```

`FromInterleaved` — geometry.cpp 내부 static helper. `Engine::Constants::GEOMETRY::VERTEX_LEN`(=13) stride 로 순회:

| 출력 필드 | interleaved 오프셋 |
|-----------|--------------------|
| `position` | raw[b+0..2] (pos.w 폐기) |
| `normal`   | raw[b+8..10] |
| `texCoord` | raw[b+11..12] |
| (color)    | raw[b+4..7] — **폐기** (사용자 승인) |

인덱스는 `GLuint` → `uint32_t` 그대로 복사. 8개 함수 전부 동일 후처리 —
변환 코드는 `FromInterleaved` 한 곳뿐.

`Plane` 매핑 근거: engine 에 `BuildPlaneIndexed` 가 없으므로 저수준
`BuildQuadIndexed` 를 `QUAD_BASE_POSITION`(z=0, 0~1 XY quad)으로 직접 호출.
`BuildConeIndexed` 의 바닥 quad 호출과 동일 패턴. 면 법선은
`ComputeFaceNormal(QUAD_BASE_POSITION[0..2])` = +Z 로, 현 `CreatePlane` 과 일치.

## 7. mesh 변경 후 모습

```cpp
// mesh.h — struct Vertex 삭제, vertex.h include 로 교체
#include "object/vertex.h"

// mesh.cpp
MeshUPtr Mesh::CreateBox()
{
    auto data = Geometry::Box();
    return Create(data.vertices, data.indices, GL_TRIANGLES);
}

MeshUPtr Mesh::CreatePlane()
{
    auto data = Geometry::Plane();
    return Create(data.vertices, data.indices, GL_TRIANGLES);
}
```

`Mesh::Create` 시그니처는 유지 (`vertices` / `indices` / `primitiveType`).
`MeshData` 를 받는 오버로드는 신설하지 않는다 (YAGNI — `CreateBox`/`CreatePlane`
가 호출부에서 풀어서 전달).

## 8. 빌드 연결 (`src/object/CMakeLists.txt`)

- `sjhopengl_object` 소스에 `geometry.cpp` 추가.
- `sjhopengl_object` 소스에 `../engine/geometry.cpp` 추가 (in-place 컴파일).
  → engine 의 다른 5개 소스(`material` / `model_base` / `shader_program` /
  `scene_graph` / `resource_management`)는 빌드하지 않아 컴파일 리스크 최소.
- `src/object/geometry.h` / `<engine>/constants.h` 는 헤더 — object 모듈의 기존
  include 경로(`src/`, `include/`)로 해석 가능, 신규 include 경로 불필요.

## 9. 알려진 영향 / 주의점

- **데이터 미세 변화**: Box/Plane 이 engine 빌더로 재도출됨. 정점 개수
  (Box 24/36, Plane 4/6)는 동일하나 normal 은 `ComputeFaceNormal` 계산값,
  UV·winding 은 engine 방식. 시각적으로 동일한 단위 큐브/평면이지만 정점
  순서·UV 가 현재와 비트 단위로 같지는 않을 수 있음. — 사용자 승인됨.
- **double-compile 잠재 리스크**: `../engine/geometry.cpp` 를 object 모듈에서
  컴파일하므로, 훗날 `src/CMakeLists.txt` 에 `add_subdirectory(engine)` 가
  활성화되면 같은 TU 가 두 번 컴파일돼 중복 심볼 발생. 현재 engine 은
  미빌드라 문제없음.
- `src/object/geometry.cpp` 는 현재 빌드되지 않는 휴면 코드 — 처음 컴파일 시
  자체 에러가 드러날 수 있음 (geometry.cpp 단일 파일만이라 리스크 최소).

## 10. 검증

- `cmake --preset ninja` 재구성 후 `SJH::object` 를 링크하는 챕터
  (chapter 들 중 mesh 소비처) 빌드 성공.
- `Mesh::CreateBox()` / `CreatePlane()` 호출 챕터를 실행해 큐브·평면이
  기존과 동일하게 렌더링되는지 육안 확인.
