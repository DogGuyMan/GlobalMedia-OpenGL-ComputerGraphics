# Slang Phase 2 — 엔진 UBO 소비 + R1 실측 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development (또는 executing-plans). Steps use checkbox (`- [ ]`).

**Goal:** Slang 이 생성한 UBO 기반 GLSL(`simple.vs`/`simple.fs`)을 엔진이 실제로 소비하도록 — `SJH::UniformBuffer` RAII + Program 의 uniform-block GL introspection + 렌더 경로 UBO 분기 — 만들고, 불릿을 PoC 로 렌더해 R1(행렬 row_major 정합)을 육안 실측한다.

**Architecture:** simple.slang 을 3블록(per-frame view/proj · per-draw model · per-material baseColor)으로 분할. Program 이 link 후 `glGetActiveUniformBlock*` 로 블록을 자기기술(이름·offset·binding)하고 블록별 `UniformBuffer` 를 소유. `MeshPassProcessor` 가 `program->HasUniformBlocks()` 면 loose `glUniform*` 대신 UBO 에 std140 기록+bind. 행렬은 **비전치 먼저**(틀리면 전치). 기존 loose 경로는 비-UBO 셰이더용으로 공존.

**Tech Stack:** C++17, OpenGL 4.1 (GLSL 410 UBO, std140), Slang `slangc`.

**정본 spec:** `doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md` (결정 D11~D13).

**검증 모델:** `no_auto_tests` — 새 단위테스트 안 만듦. 검증 = 빌드 GREEN + grep + **Task 5 사용자 육안**(R1 게이트). 빌드/실행은 사용자 또는 에이전트가 `export PATH="$HOME/slang/bin:$PATH"` 후 수행.

**커밋 규칙:** PATH-SCOPED partial 커밋(`git commit <경로> -m`). `git add -A`/bare commit 금지. `Co-Authored-By` 미사용. Korean, prefix `[shader]`/`[engine]`/`[build]`.

---

## 파일 구조

**생성:**
- `src/program/uniform_buffer.h` / `src/program/uniform_buffer.cpp` — `SJH::UniformBuffer` (GL_UNIFORM_BUFFER RAII + Update + BindBase). **`src/program/` 에 둠** — `SJH::buffer` 가 이미 `SJH::program` 에 PUBLIC 의존(buffer→program)하므로 buffer 에 두면 program↔buffer 사이클. program 모듈 내부에 두어 사이클 회피(program 이 유일 소비자).

**수정:**
- `apps/_MyApp_/shaders_slang/simple.slang` — 1블록 → 3블록 분할.
- `src/program/program.h` / `program.cpp` — uniform-block introspection + UBO 소유 + Has/Get/Update/Bind API.
- `src/program/CMakeLists.txt` — uniform_buffer.cpp 소스 추가 (새 link 불요 — 동일 모듈).
- `<src>/render/mesh_pass_processor.cpp` — WorldMesh 분기에 UBO 경로.
- `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` — MeshRenderer 활성화(PoC 렌더).

**불변:** 기존 loose-uniform 경로(비-UBO 셰이더), `LightUniformDispatcher`, 다른 셰이더(phong 등) — 손대지 않음.

---

## Task 1: simple.slang 3블록 분할

**Files:** Modify `apps/_MyApp_/shaders_slang/simple.slang`

per-frame/per-draw/per-material 갱신 빈도별 3 ConstantBuffer 로 분할 (spec D11).

- [ ] **Step 1: 전체 교체**

```hlsl
// apps/_MyApp_/shaders_slang/simple.slang
// 기존 simple.vs + simple.fs 등가 (라이팅 무관 단색). 3블록 분할(D11):
//   FrameBlock(per-frame: view/proj) / DrawBlock(per-draw: model) / MaterialBlock(per-material: baseColor)
// 생성물: simple.vs (vsMain) / simple.fs (fsMain). GLSL 블록명 = block_FrameBlock_0 등.

struct FrameBlock    { float4x4 uView; float4x4 uProj; };
struct DrawBlock     { float4x4 uModel; };
struct MaterialBlock { float4 baseColor; };

ConstantBuffer<FrameBlock>    uFrame;
ConstantBuffer<DrawBlock>     uDraw;
ConstantBuffer<MaterialBlock> uMaterial;

struct VSIn { float3 aPos : POSITION; };

[shader("vertex")]
float4 vsMain(VSIn input) : SV_Position
{
    return mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0))));
}

[shader("fragment")]
float4 fsMain() : SV_Target
{
    return uMaterial.baseColor;
}
```

- [ ] **Step 2: 빌드 + 생성물 검증**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --build --preset ninja --target _MyApp__shaders 2>&1 | tail -5
echo "=== VS blocks (FrameBlock + DrawBlock 둘 다) ==="
grep -nE "#version|uniform block_" build_ninja/apps/_MyApp_/slang_generated/shaders/simple.vs
echo "=== FS block (MaterialBlock) ==="
grep -nE "#version|uniform block_" build_ninja/apps/_MyApp_/slang_generated/shaders/simple.fs
echo "=== binding 잔존 0 ==="
grep -c "layout(binding" build_ninja/apps/_MyApp_/slang_generated/shaders/simple.vs build_ninja/apps/_MyApp_/slang_generated/shaders/simple.fs
```
Expected: simple.vs 첫줄 `#version 410 core` + `block_FrameBlock_0` + `block_DrawBlock_0`; simple.fs 에 `block_MaterialBlock_0`; binding 카운트 0/0.

- [ ] **Step 3: 커밋**
```bash
git commit apps/_MyApp_/shaders_slang/simple.slang -m "[shader] : simple.slang 3블록 분할 (Frame/Draw/Material)"
```

---

## Task 2: SJH::UniformBuffer RAII

**Files:** Create `src/program/uniform_buffer.h`, `src/program/uniform_buffer.cpp`; Modify `src/program/CMakeLists.txt`

GL_UNIFORM_BUFFER RAII — Buffer(VBO/EBO) 패턴과 동일 컨벤션(팩토리 + UPtr + `Get()`). **`src/program/` 에 둠**(buffer→program 기존 의존 때문에 buffer 에 두면 사이클).

- [ ] **Step 1: `src/program/uniform_buffer.h`**

```cpp
/**
 * @file uniform_buffer.h
 * @brief UBO (GL_UNIFORM_BUFFER) RAII 래퍼 — std140 블록 데이터 업로드 + binding point 바인딩.
 * @details Buffer(VBO/EBO) 와 분리 — UBO 는 binding-point 기반(glBindBufferBase) 이라 사용법이 다름.
 *          Slang 생성 GLSL 의 uniform block 을 채우는 데 사용 (Phase 2).
 */
#ifndef __SJH_UNIFORM_BUFFER_H__
#define __SJH_UNIFORM_BUFFER_H__

#include "common/common.h"
#include "GL/gl3w.h"
#include <cstddef>

namespace SJH
{
    CLASS_PTR(UniformBuffer)

    /// @brief GL_UNIFORM_BUFFER RAII — 고정 크기 할당(GL_DYNAMIC_DRAW) 후 부분 갱신/바인딩.
    class UniformBuffer
    {
    public:
        /// @brief size 바이트의 UBO 를 생성(내용 미초기화, GL_DYNAMIC_DRAW). 실패 시 nullptr.
        static UniformBufferUPtr Create(size_t size);

        ~UniformBuffer();
        UniformBuffer(const UniformBuffer&)            = delete;
        UniformBuffer& operator=(const UniformBuffer&) = delete;
        UniformBuffer(UniformBuffer&&)                 = delete;
        UniformBuffer& operator=(UniformBuffer&&)      = delete;

        /// @brief data 의 bytes 바이트를 offset 위치에 업로드 (glBindBuffer + glBufferSubData).
        void Update(const void* data, size_t bytes, size_t offset = 0) const;

        /// @brief 이 UBO 를 binding point 에 결속 (glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, handle)).
        void BindBase(GLuint bindingPoint) const;

        GLuint Get() const { return mBuffer; }
        size_t Size() const { return mSize; }

    private:
        UniformBuffer() = default;
        bool Init(size_t size);

        GLuint mBuffer{0};
        size_t mSize{0};
    };
} // namespace SJH
#endif // __SJH_UNIFORM_BUFFER_H__
```

- [ ] **Step 2: `src/program/uniform_buffer.cpp`**

```cpp
/**
 * @file uniform_buffer.cpp
 * @brief UniformBuffer 구현 — glGenBuffers/glBufferData(NULL)/glBufferSubData/glBindBufferBase.
 */
#include "src/program/uniform_buffer.h"

namespace SJH
{
    UniformBufferUPtr UniformBuffer::Create(size_t size)
    {
        auto ubo = UniformBufferUPtr(new UniformBuffer());
        if (!ubo->Init(size))
            return nullptr;
        return ubo;
    }

    bool UniformBuffer::Init(size_t size)
    {
        mSize = size;
        glGenBuffers(1, &mBuffer);
        if (mBuffer == 0)
            return false;
        glBindBuffer(GL_UNIFORM_BUFFER, mBuffer);
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        return true;
    }

    UniformBuffer::~UniformBuffer()
    {
        if (mBuffer != 0)
            glDeleteBuffers(1, &mBuffer);
    }

    void UniformBuffer::Update(const void* data, size_t bytes, size_t offset) const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, mBuffer);
        glBufferSubData(GL_UNIFORM_BUFFER, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(bytes), data);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void UniformBuffer::BindBase(GLuint bindingPoint) const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, bindingPoint, mBuffer);
    }
} // namespace SJH
```

- [ ] **Step 3: `src/buffer/CMakeLists.txt` 에 소스 추가**

기존 STATIC 라이브러리의 소스 목록(예: `add_library(... buffer.cpp)`)에 `uniform_buffer.cpp` 를 추가. 헤더-only 나열이 없으면 `.cpp` 만 추가하면 됨. (정확한 형식은 기존 파일 따름.)

- [ ] **Step 4: 빌드 GREEN (buffer 모듈만)**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --preset ninja >/dev/null 2>&1
cmake --build --preset ninja --target buffer 2>&1 | tail -8
echo "EXIT=${PIPESTATUS[0]}"
```
Expected: `EXIT=0` (buffer 라이브러리에 uniform_buffer.cpp 컴파일됨). 타겟명이 `buffer` 가 아니면 `cmake --build --preset ninja 2>&1 | grep -i uniform_buffer` 로 컴파일 확인.

- [ ] **Step 5: 커밋**
```bash
git add src/program/uniform_buffer.h src/program/uniform_buffer.cpp
git commit src/program/uniform_buffer.h src/program/uniform_buffer.cpp src/buffer/CMakeLists.txt -m "[engine] : SJH::UniformBuffer RAII (UBO std140 업로드)"
```

---

## Task 3: Program uniform-block introspection + UBO 소유

**Files:** Modify `src/program/program.h`, `src/program/program.cpp`, `src/program/CMakeLists.txt`

link 후 active uniform block 을 GL introspection 으로 자기기술하고, 블록별 `UniformBuffer` 를 소유. binding point 결속까지 수행.

**설계:**
- 블록 식별: `glGetProgramiv(GL_ACTIVE_UNIFORM_BLOCKS)` → 각 블록 `glGetActiveUniformBlockName` (예: `block_FrameBlock_0`) + `glGetActiveUniformBlockiv(GL_UNIFORM_BLOCK_DATA_SIZE)`.
- 정규화: `block_<Struct>_0` → `Struct` (접두 `block_` 제거 + 마지막 `_0`/`_숫자` 제거). 렌더 경로가 "FrameBlock"/"DrawBlock"/"MaterialBlock" 로 조회.
- binding point: 블록 순서 index 를 그대로 binding point 로 사용, `glUniformBlockBinding(prog, blockIndex, bindingPoint)`.
- UBO: 블록별 `UniformBuffer::Create(dataSize)`.
- 멤버 offset: PoC 범위는 **std140 알려진 레이아웃 사용**(FrameBlock: uView@0 / uProj@64, DrawBlock: uModel@0, MaterialBlock: baseColor@0; mat4=64B). 렌더 경로가 이 offset 으로 write. (멤버 introspection 은 Phase 3 일반화 시.)

- [ ] **Step 1: `program.h` 에 블록 구조체 + API 추가**

`#include` 에 `#include "src/program/uniform_buffer.h"` + `#include <vector>` + `#include <string>` 확인. 클래스 `public:` 에 추가:
```cpp
        /// @brief Slang 생성 UBO 블록 1개의 자기기술 + 소유 UBO.
        struct UniformBlock
        {
            std::string         normalizedName; ///< "FrameBlock"/"DrawBlock"/"MaterialBlock" (block_<X>_0 정규화)
            GLuint              blockIndex{0};   ///< glGetActiveUniformBlock 인덱스
            GLuint              bindingPoint{0}; ///< glUniformBlockBinding 으로 결속한 point
            GLint               dataSize{0};     ///< GL_UNIFORM_BLOCK_DATA_SIZE
            UniformBufferUPtr   ubo;             ///< 이 블록 데이터를 담는 UBO (Program 소유)
        };

        /// @brief 이 프로그램이 active uniform block 을 가지는가 (Slang UBO 셰이더 식별).
        bool HasUniformBlocks() const { return !mUniformBlocks.empty(); }

        /// @brief 정규화 이름으로 블록 조회 (없으면 nullptr).
        const UniformBlock* FindUniformBlock(const std::string& normalizedName) const;

        /// @brief 모든 블록을 각자의 binding point 에 BindBase (드로우 전 1회).
        void BindUniformBlocks() const;

        /// @brief 정규화 이름 블록의 UBO 에 부분 업로드 (offset/bytes 는 호출자가 std140 으로 계산).
        void UpdateUniformBlock(const std::string& normalizedName, const void* data, size_t bytes, size_t offset) const;
```
그리고 `private:` 멤버에:
```cpp
        /// @brief link 후 introspect 한 UBO 블록들 (Program 소유). 비-UBO 셰이더는 빈 벡터.
        std::vector<UniformBlock> mUniformBlocks;

        /// @brief active uniform block 열거 + binding 결속 + UBO 생성. Create 가 link 직후 호출.
        void BuildUniformBlocks();
```

- [ ] **Step 2: `program.cpp` 에 introspection 구현**

`Create()` 에서 `mUniformCache.Build(*program);` **직후** 한 줄 추가:
```cpp
        program->BuildUniformBlocks();   // Phase 2 — Slang UBO 블록 자기기술
```
그리고 익명/SJH namespace 안에 구현 추가:
```cpp
    namespace
    {
        // "block_FrameBlock_0" -> "FrameBlock" (접두 block_ 제거 + 마지막 _<digits> 제거)
        std::string NormalizeBlockName(const std::string& raw)
        {
            std::string s = raw;
            const std::string prefix = "block_";
            if (s.rfind(prefix, 0) == 0)
                s = s.substr(prefix.size());
            // 마지막 "_<숫자>" 접미 제거
            size_t us = s.find_last_of('_');
            if (us != std::string::npos && us + 1 < s.size())
            {
                bool allDigit = true;
                for (size_t i = us + 1; i < s.size(); ++i)
                    if (!std::isdigit(static_cast<unsigned char>(s[i]))) { allDigit = false; break; }
                if (allDigit) s = s.substr(0, us);
            }
            return s;
        }
    }

    void Program::BuildUniformBlocks()
    {
        GLint numBlocks = 0;
        glGetProgramiv(mProgramAddr, GL_ACTIVE_UNIFORM_BLOCKS, &numBlocks);
        for (GLint i = 0; i < numBlocks; ++i)
        {
            GLint nameLen = 0;
            glGetActiveUniformBlockiv(mProgramAddr, i, GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
            std::string rawName(static_cast<size_t>(nameLen > 0 ? nameLen - 1 : 0), '\0');
            GLsizei written = 0;
            glGetActiveUniformBlockName(mProgramAddr, i, nameLen, &written,
                                        rawName.empty() ? nullptr : &rawName[0]);

            GLint dataSize = 0;
            glGetActiveUniformBlockiv(mProgramAddr, i, GL_UNIFORM_BLOCK_DATA_SIZE, &dataSize);

            UniformBlock blk;
            blk.normalizedName = NormalizeBlockName(rawName);
            blk.blockIndex     = static_cast<GLuint>(i);
            blk.bindingPoint   = static_cast<GLuint>(i);   // index 를 그대로 binding point 로
            blk.dataSize       = dataSize;
            glUniformBlockBinding(mProgramAddr, blk.blockIndex, blk.bindingPoint);
            blk.ubo = UniformBuffer::Create(static_cast<size_t>(dataSize));
            mUniformBlocks.push_back(std::move(blk));
        }
    }

    const Program::UniformBlock* Program::FindUniformBlock(const std::string& normalizedName) const
    {
        for (const auto& b : mUniformBlocks)
            if (b.normalizedName == normalizedName)
                return &b;
        return nullptr;
    }

    void Program::BindUniformBlocks() const
    {
        for (const auto& b : mUniformBlocks)
            if (b.ubo) b.ubo->BindBase(b.bindingPoint);
    }

    void Program::UpdateUniformBlock(const std::string& normalizedName, const void* data, size_t bytes, size_t offset) const
    {
        const UniformBlock* b = FindUniformBlock(normalizedName);
        if (b && b->ubo) b->ubo->Update(data, bytes, offset);
    }
```
`program.cpp` 상단 include 에 `#include <cctype>` 추가 (isdigit).

- [ ] **Step 3: `src/program/CMakeLists.txt` 에 buffer link 추가**

`target_link_libraries(program ...)` (또는 해당 타겟명) 에 `SJH::buffer` 를 PUBLIC 추가 (program.h 가 `src/program/uniform_buffer.h` 를 노출하므로 PUBLIC).

- [ ] **Step 4: 빌드 GREEN (program 모듈)**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --preset ninja >/dev/null 2>&1
cmake --build --preset ninja --target program 2>&1 | tail -10
echo "EXIT=${PIPESTATUS[0]}"
```
Expected: `EXIT=0`. (static_assert 들 통과 — RAII 의미론 유지. mUniformBlocks 는 move-only UPtr 멤버라 Program 의 non-copy/non-move 와 일관.)

- [ ] **Step 5: 커밋**
```bash
git add src/program/program.h src/program/program.cpp
git commit src/program/program.h src/program/program.cpp src/program/CMakeLists.txt -m "[engine] : Program UBO 블록 introspection + UniformBuffer 소유 (GL 자기기술)"
```

---

## Task 4: 렌더 경로 UBO 분기 (비전치)

**Files:** Modify `<src>/render/mesh_pass_processor.cpp` (WorldMesh 분기, [<src>/render/mesh_pass_processor.cpp:151-177])

`program->HasUniformBlocks()` 면 loose `SetMat4` 대신 UBO 에 std140 기록 + bind. **행렬 비전치**(vmath column-major raw 바이트 그대로) 먼저.

- [ ] **Step 1: WorldMesh 분기 수정**

기존 (결정1/2/4) 의 loose 송신을 UBO 분기로 감싼다. [mesh_pass_processor.cpp:151-177] 의 세 블록을 다음으로 교체:

```cpp
            const bool useUbo = program->HasUniformBlocks();

            // 결정 1: Program 전환 - view/proj
            if (program != lastProg) {
                rc.UseProgram(*program);
                if (useUbo) {
                    // FrameBlock: std140 { mat4 uView @0; mat4 uProj @64; } — 비전치 raw 바이트(D13)
                    program->UpdateUniformBlock("FrameBlock", &viewMat, sizeof(vmath::mat4), 0);
                    program->UpdateUniformBlock("FrameBlock", &projMat, sizeof(vmath::mat4), sizeof(vmath::mat4));
                    program->BindUniformBlocks();
                } else {
                    if (program->GetLocation(Const::UNI_VIEW) >= 0)
                        Uniforms::SetMat4(*program, Const::UNI_VIEW, viewMat);
                    if (program->GetLocation(Const::UNI_PROJ) >= 0)
                        Uniforms::SetMat4(*program, Const::UNI_PROJ, projMat);
                }
                lastProg = program;
                lastMat  = nullptr;
            }

            // 결정 2: Material 전환
            if (material != lastMat) {
                if (useUbo) {
                    // MaterialBlock: std140 { vec4 baseColor @0; } — Material PropertyBlock 에서 추출
                    auto it = material->Properties.Vec4s.find("baseColor");
                    vmath::vec4 base = (it != material->Properties.Vec4s.end()) ? it->second
                                                                               : vmath::vec4(1,1,1,1);
                    program->UpdateUniformBlock("MaterialBlock", &base, sizeof(vmath::vec4), 0);
                }
                // loose 머티리얼 유니폼/텍스처는 항상 시도 (UBO 셰이더는 해당 cache entry 가 없어 자연 skip)
                PropertyBlockSetter::Set(rc, material->Properties, *program);
                lastMat = material;
            }

            const Pass::PipelineState passState = Pass::DefaultPipelineStateOf(material->GetPass());
            stateSetter.Set(passState);

            // 결정 4: model + draw
            if (useUbo) {
                // DrawBlock: std140 { mat4 uModel @0; } — 비전치
                program->UpdateUniformBlock("DrawBlock", &cmd.modelMatrix, sizeof(vmath::mat4), 0);
            } else {
                if (program->GetLocation(Const::UNI_MODEL) >= 0)
                    Uniforms::SetMat4(*program, Const::UNI_MODEL, cmd.modelMatrix);
            }
            rc.BindVAO(mesh->GetVAO());
            rc.DrawIndexed(mesh->GetIndexCount());
```

`mesh_pass_processor.cpp` 상단에 `#include "material/material_property_block.h"` 가 이미 없으면 추가(Properties.Vec4s 접근용 — 보통 material.h 경유로 이미 포함). `vmath` 헤더도 이미 포함됨(viewMat 인자).

- [ ] **Step 2: 빌드 GREEN (render 모듈 + _MyApp_)**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --preset ninja >/dev/null 2>&1
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -10
echo "EXIT=${PIPESTATUS[0]}"
```
Expected: `EXIT=0`. (UBO 분기 컴파일. 아직 불릿 MeshRenderer 비활성이라 런타임 동작 변화 없음 — 다음 Task 에서 활성.)

- [ ] **Step 3: 커밋**
```bash
git commit <src>/render/mesh_pass_processor.cpp -m "[engine] : 렌더 경로 UBO 분기 (HasUniformBlocks 시 std140 기록, 비전치 D13)"
```

---

## Task 5: 불릿 PoC 렌더 + R1 육안 게이트

**Files:** Modify `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` (MeshRenderer 활성화 — 현재 주석)

불릿이 overlaid 된 slang `simple.vs/fs`(UBO) 로 실제 렌더되게 하여 R1(행렬) 실측.

- [ ] **Step 1: MeshRenderer 활성화**

`apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` 에서 주석 처리된 MeshRenderer 추가 라인(대략):
```cpp
// actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
```
를 주석 해제:
```cpp
actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);
```
(정확한 변수명 `mesh`/`mat`/`actor` 는 해당 파일 문맥을 따름. MeshRenderer 가 mesh+material 을 받는 시그니처인지 확인 후 일치시킬 것.)

- [ ] **Step 2: 빌드 + 실행 (사용자 육안 — R1 게이트)**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
echo "EXIT=${PIPESTATUS[0]}"
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
**사용자 육안 확인 (R1):** 발사한 불릿(마젠타 구, 반경 0.15)이
- ✅ **정상 위치·크기로 플레이어에서 발사되어 날아가면 → R1 통과 (비전치 정답), Task 6 SKIP.**
- ❌ 화면에서 사라짐/찌그러짐/엉뚱한 위치 → 행렬 전치 필요 → **Task 6 진행.**

- [ ] **Step 3: 커밋**
```bash
git commit apps/_MyApp_/src/Entity/Bullet/bullet_factory.h -m "[shader] : 불릿 MeshRenderer 활성화 — slang UBO simple 셰이더 PoC 렌더"
```

---

## Task 6 (조건부 — Task 5 가 ❌ 일 때만): 행렬 전치

**Files:** Modify `<src>/render/mesh_pass_processor.cpp`

Task 5 육안이 ❌(찌그러짐/소실)이면 Slang row_major UBO 가 전치된 바이트를 기대하는 것. UBO 기록 전 mat4 를 전치.

- [ ] **Step 1: 전치 헬퍼로 기록**

`mesh_pass_processor.cpp` 의 UBO 분기에서 `viewMat`/`projMat`/`cmd.modelMatrix` 를 직접 넘기는 대신 전치본을 넘긴다. vmath 의 transpose 사용(헤더에 `transpose()` 자유함수/멤버 존재 — 없으면 `vmath::transpose(m)`):
```cpp
                    vmath::mat4 vT = viewMat.transpose();   // vmath transpose API 확인 후 일치
                    vmath::mat4 pT = projMat.transpose();
                    program->UpdateUniformBlock("FrameBlock", &vT, sizeof(vmath::mat4), 0);
                    program->UpdateUniformBlock("FrameBlock", &pT, sizeof(vmath::mat4), sizeof(vmath::mat4));
```
model 도 동일:
```cpp
                vmath::mat4 mT = cmd.modelMatrix.transpose();
                program->UpdateUniformBlock("DrawBlock", &mT, sizeof(vmath::mat4), 0);
```

- [ ] **Step 2: 빌드 + 재실행 육안**

Run:
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
export PATH="$HOME/slang/bin:$PATH"
cmake --build --preset ninja --target _MyApp_ 2>&1 | tail -5
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
Expected: 이제 불릿이 정상 위치로 렌더 → R1 = **전치 필요** 로 확정.

- [ ] **Step 3: 커밋 + spec D13 갱신**
```bash
git commit <src>/render/mesh_pass_processor.cpp -m "[engine] : R1 확정 — UBO 행렬 전치 기록 (Slang row_major 정합)"
```
그리고 spec `doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md` 의 R1/D13 을 "전치 필요 확정"으로 갱신(별도 커밋 불요 — docs gitignore).

---

## Self-Review

**Spec coverage:** D11(3블록)→T1 ✓ / D12(GL introspection, JSON 불용)→T3 ✓ / D13(비전치 먼저→틀리면 전치)→T4+T6 ✓ / spec §4.3 엔진 UBO→T2+T3+T4 ✓ / Phase 2 = 이 플랜 ✓. 라이트/머티리얼 UBO 일반화·26종 이주 = Phase 3 (범위 밖, 명시).

**Placeholder scan:** 모든 코드 블록 실내용. introspection 의 멤버 offset 은 std140 알려진 레이아웃(mat4=64B contiguous)으로 명시. 없음.

**Type 일관성:** `UniformBuffer::Create/Update/BindBase/Get/Size` (T2) ↔ `Program::UniformBlock.ubo` 사용(T3) ↔ `program->UpdateUniformBlock/BindUniformBlocks/HasUniformBlocks/FindUniformBlock`(T3) ↔ 렌더 호출(T4) 시그니처 일치. 정규화 이름 "FrameBlock"/"DrawBlock"/"MaterialBlock" = simple.slang struct 명(T1)과 일치. ✓

**리스크:** (a) `MeshRenderer` 생성자 시그니처 — bullet_factory 가 mesh+material 받는지 실제 확인(T5 주의). (b) baseColor 가 UBO 화되어 PropertyBlockSetter 가 못 찾음 → UBO 분기가 직접 주입(T4 결정2)으로 보전. (c) std140 에서 mat4/vec4 는 16B 정렬이라 FrameBlock uProj@64·MaterialBlock baseColor@0 가정 유효(dataSize 로 교차검증 가능). (d) R1 = 육안 게이트, Task 6 가 fallback.
