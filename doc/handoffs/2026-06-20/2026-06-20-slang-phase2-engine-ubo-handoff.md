 # HANDOFF — Slang Phase 2: 엔진 UBO 소비 + R1 실측

> **수신자:** 신규 Claude Code 세션/에이전트 (이 대화의 컨텍스트 없음 가정).
> **작성:** 2026-06-20 · **상태:** Phase 1 완료·머지됨, Phase 2 미착수(설계만 완료, 코드 0).
> 이 문서 하나로 Phase 2 를 안전하게 수행 가능하도록 자기완결로 작성. 첨부 spec/plan 은 *선택적 깊이*.

---

## 0. TL;DR + 다음 액션

Slang 으로 셰이더를 작성→GLSL 트랜스파일하는 마이그레이션 중. **Phase 1(빌드타임 slangc 툴체인)은 완료·머지됨.** Slang 의 GLSL 출력은 전역 유니폼을 **std140 UBO 블록**으로 강제하는데, 현재 엔진은 loose `glUniform*` 이름 송신이라 **엔진이 UBO 를 소비하도록 만드는 것이 Phase 2.**

**다음 액션:** 새 브랜치에서 (1) `SJH::UniformBuffer` RAII → (2) `Program` 의 UBO GL-introspection + 소유 → (3) `MeshPassProcessor` UBO 분기 → (4) 불릿을 PoC 로 렌더해 **R1(행렬 정합)을 사용자 육안 실측.** 상세 6-스텝은 §6.

**⚠ 게이트 = R1:** Slang GLSL 은 `layout(row_major)` UBO 를 내보내는데 엔진 `vmath::mat4` 는 column-major. **전치 필요 여부가 미해결** — 코드로는 못 풀고 실행→육안으로만 확정. "비전치 먼저 시도, 틀리면 전치"가 확정 방침(D13).

---

## 1. State of the world (재측정 2026-06-20)

- **현재 브랜치:** `game/main`. Phase 2 는 **여기서 새 브랜치**(예: `game/slang-phase2-ubo`)로 시작.
- **Phase 1 머지 완료** — `game/main` HEAD 부근 squash 커밋 `90353f4 [build] : Slang GLSL 410 ...`. 포함 파일(전부 game/main 에 존재, `git cat-file -e HEAD:<path>` 로 확인됨):
  - `cmake/Slang.cmake` — `find_program(SLANGC_EXECUTABLE)`(미발견 시 WARNING+graceful skip, FATAL 아님) + 함수 `sjh_compile_slang(OUT_VAR INPUT ENTRY STAGE SLANG_TARGET PROFILE OUT_FILE)` + `sjh_reflect_slang(...)`.
  - `cmake/SlangPostProcess410.cmake` — 생성 GLSL 을 410 호환화(`#version 450`→`410 core`, `layout(binding=N)` 제거, `layout(...) buffer;` 제거).
  - `apps/_MyApp_/shaders_slang/simple.slang` — **현재 game/main 엔 1블록 버전.** (3블록 분할본은 파킹 브랜치에만 — §6 T1 에서 재적용.)
  - `apps/_MyApp_/CMakeLists.txt` — `if(SLANGC_EXECUTABLE)` 가드 하에 `simple.slang`→`simple.vs/fs`(+wgsl+refl) 생성, 빌드 후 `build_ninja/apps/_MyApp_/slang_generated/shaders/` 로 떨어지고 POST_BUILD 가 실행파일 옆 `resources/shaders/` 에 **overlay 복사**(원본 GLSL 덮어씀).
  - root `CMakeLists.txt` — `include(cmake/Slang.cmake)`.
- **파킹 브랜치 `game/slang-phase2`** — 커밋 `911ca37 [shader] : simple.slang 3블록 분할` 1개만(엔진 코드 0). 참고용. 새로 시작하면 무시 가능(§6 T1 이 동일 내용 재적용).
- **엔진 UBO 코드: 전무.** `src/program/uniform_buffer.*` 없음, `HasUniformBlocks`/`UpdateUniformBlock` 등 grep 0. 즉 **롤백할 렌더링 아키텍처 코드 없음 — greenfield.**
- **slangc:** `~/slang/bin/slangc` (v2026.11, GitHub release). 빌드 전 `export PATH="$HOME/slang/bin:$PATH"` 필수.

---

## 2. Locked decisions (재논의 금지)

| # | 결정 | 근거(실측) |
|---|---|---|
| D11 | UBO 블록 **3분할**: per-frame(view/proj) / per-draw(model) / per-material(baseColor) | 갱신 빈도별. simple.slang 의 3 ConstantBuffer. |
| D12 | UBO 식별 = **Program GL introspection** (`glGetActiveUniformBlock*`) | 런타임 nlohmann-json 불용(GL 이 블록 size 제공). 블록명 `block_<Struct>_0` 정규화로 매핑. refl.json 은 빌드 산출물로만 잔존. |
| D13 | 행렬 **비전치 먼저**, 틀리면 전치 | Slang GLSL=`layout(row_major)` vs vmath=column-major. 코드로 단정 불가 → 육안 게이트. |
| (정정) | `UniformBuffer` 는 **`src/program/` 에 둔다** (`src/buffer/` 아님) | **`src/buffer/CMakeLists.txt:17` 이 이미 `SJH::buffer`→`SJH::program` PUBLIC 의존.** buffer 에 두고 program 이 link 하면 **program↔buffer 사이클**. program 모듈 내부에 두어 회피(program 이 유일 소비자). |

**Slang→GLSL 핵심 실측(왜 Phase 2 가 필요한가):** Slang 은 loose uniform 보존 불가 — 전역/ConstantBuffer 를 항상 `layout(std140) uniform block_<X>_0 {...}` 로 래핑(`glGetUniformLocation("uModel")` = -1). 텍스처는 combined `Sampler2D` 쓰면 `uniform sampler2D`(410 합법) 보존. 같은 `.slang` 이 `-target wgsl` 로 WGSL 도 산출(미래 WebGPU). 자세한 PoC 로그는 spec(§3) 참조.

---

## 3. Verified integration seams (file:line, 2026-06-20 재확인)

- **렌더 드로우 루프:** [`src/render/mesh_pass_processor.cpp:95-182`](../../src/render/mesh_pass_processor.cpp#L95) `MeshPassProcessor::Process(rc, viewMat, projMat)`. WorldMesh 분기 `:142-177`.
  - `rc.UseProgram(*program)` `:153`. view/proj loose 송신 `:154-157`. material 송신 `PropertyBlockSetter::Set` `:164`. model loose 송신 `:174-175`. draw `:176-177`. **여기에 `program->HasUniformBlocks()` 분기 삽입.**
- **Program 팩토리:** [`src/program/program.cpp:44-53`](../../src/program/program.cpp#L44) `Create()` — `mUniformCache.Build(*program)` `:51` **직후** `BuildUniformBlocks()` 호출. RAII: 복사/이동 전부 `=delete`(`:83-86` in program.h) — UBO 멤버는 move-only UPtr 로 일관 유지.
- **행렬 규약:** `vmath::mat4` = **column-major** (`include/vmath.h:878` `vecN data[w]`), 업로드 `glUniformMatrix4fv(loc,1,GL_FALSE,...)` ([`src/program/program_uniforms.cpp:53`](../../src/program/program_uniforms.cpp#L53)). 전치 멤버 `mat4::transpose()` 존재 (`include/vmath.h:845`).
- **사이클 가드:** [`src/buffer/CMakeLists.txt:17`](../../src/buffer/CMakeLists.txt#L17) `SJH::buffer` PUBLIC `SJH::program` → UniformBuffer 를 buffer 에 두지 말 것(§2 정정).
- **불릿 PoC 타깃:** [`apps/_MyApp_/src/Entity/Bullet/bullet_factory.h:133`](../../apps/_MyApp_/src/Entity/Bullet/bullet_factory.h#L133) — `//         actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);` **주석 처리됨**. 프로그램은 `reg.CreateProgram(..., "./resources/shaders/simple.vs", "./resources/shaders/simple.fs")`(= overlay 된 slang UBO 셰이더), 머티리얼 `Properties.Vec4s["baseColor"]` = 마젠타. 주석 해제하면 불릿이 UBO 셰이더로 렌더 → R1 실측. (정확한 `mesh`/`mat`/`actor` 변수명·MeshRenderer 시그니처는 파일에서 재확인.)
- **PropertyBlockSetter:** [`src/render/property_block_setter.cpp`](../../src/render/property_block_setter.cpp) — uniform cache schema 순회로 송신. baseColor 가 UBO 멤버가 되면 cache 에 없어 자연 skip → UBO 분기가 직접 주입해야 함(§6 T4 결정2).

---

## 4. Guardrails & conventions (반드시 준수)

- **커밋:** 사용자가 같은 트리에서 **병렬 git 작업**. 반드시 **PATH-SCOPED partial 커밋**(`git commit <경로...> -m`). `git add -A`/bare `git commit` **금지**(사용자 staged 작업 휩쓺). `Co-Authored-By` 트레일러 **미사용**. 메시지 한국어, prefix `[engine]`/`[shader]`/`[build]`.
- **빌드/검증:** `export PATH="$HOME/slang/bin:$PATH"` 후 `cmake --preset ninja` + `cmake --build --preset ninja --target _MyApp_`. 모듈 단독: `--target program` 등. **테스트 자동작성 금지(`no_auto_tests`)** — 검증 = 빌드 GREEN + grep + 육안.
- **코드 컨벤션:** 주석 한국어 + Doxygen + ASCII/한글만(특수문자 0). 헤더가드 `__SJH_X_H__`(`#pragma once` 미사용). 명명 멤버 `mPascalCase`/지역 `camelCase`/타입·함수 `PascalCase`/bool `mIs*`/포인터 `*Ptr`, trailing underscore 미사용.
- **크로스플랫폼:** `long` 금지(`int32_t`/`uint64_t`), 경로 슬래시, `windows.h` 는 `#ifdef _WIN32` 안에서만.
- **스코프 — 건드리지 말 것:** `LightUniformDispatcher`, 다른 셰이더(phong/postprocess 등), 기존 loose-uniform 경로(비-UBO 셰이더용으로 **공존**), `cmake/Slang*.cmake`(Phase 1 완성품), sb7code. Phase 2 는 **simple/불릿 PoC 한정** — phong/라이트/머티리얼 일반 UBO 화는 Phase 3.
- **R1 은 사용자만 확정 가능:** 비주얼 게이트(§6 T5). 에이전트가 임의로 "전치 맞다/아니다" 단정 금지 — 실행 후 사용자 육안 보고를 받아 T6 분기.

---

## 5. 충돌 매트릭스 (단독 작업 가정, 신규 파일 위주)

| 파일 | 소유 | 비고 |
|---|---|---|
| `src/program/uniform_buffer.{h,cpp}` | 신규(이 작업) | 충돌 없음 |
| `src/program/program.{h,cpp}` + CMakeLists | 이 작업 | 기존 파일 수정 — surgical, 기존 API 보존 |
| `src/render/mesh_pass_processor.cpp` | 이 작업 | 기존 드로우 루프에 분기 추가 — loose 경로 보존 |
| `apps/_MyApp_/shaders_slang/simple.slang` | 이 작업 | 1블록→3블록 |
| `apps/_MyApp_/src/Entity/Bullet/bullet_factory.h` | 이 작업 | 주석 1줄 해제 |
| 그 외 src/render, LightUniformDispatcher, phong 셰이더 | **건드리지 말 것** | 공존/Phase 3 |

---

## 6. 구현 슬라이스 — 6 Task (paste-ready)

> 전체 코드·검증 명령은 아래에 인라인. 더 깊은 맥락은 (같은 머신이면) gitignore 로컬 plan
> `doc/superpowers/plans/2026-06-20-slang-phase2-engine-ubo.md` + spec `doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md` 참조(선택).

### T1 — simple.slang 3블록 분할
`apps/_MyApp_/shaders_slang/simple.slang` 전체를 아래로 교체:
```hlsl
// 3블록 분할(D11): FrameBlock(view/proj)/DrawBlock(model)/MaterialBlock(baseColor)
struct FrameBlock    { float4x4 uView; float4x4 uProj; };
struct DrawBlock     { float4x4 uModel; };
struct MaterialBlock { float4 baseColor; };
ConstantBuffer<FrameBlock>    uFrame;
ConstantBuffer<DrawBlock>     uDraw;
ConstantBuffer<MaterialBlock> uMaterial;
struct VSIn { float3 aPos : POSITION; };
[shader("vertex")]
float4 vsMain(VSIn input) : SV_Position
{ return mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0)))); }
[shader("fragment")]
float4 fsMain() : SV_Target { return uMaterial.baseColor; }
```
검증: `cmake --build --preset ninja --target _MyApp__shaders` 후 `grep "uniform block_" build_ninja/apps/_MyApp_/slang_generated/shaders/simple.vs`(=Frame+Draw), `...simple.fs`(=Material), `grep -c "layout(binding" ...`(=0). 커밋 `[shader] : simple.slang 3블록 분할`.

### T2 — `SJH::UniformBuffer` RAII (★ `src/program/` 에 둘 것 — 사이클 회피)
`src/program/uniform_buffer.h`:
```cpp
#ifndef __SJH_UNIFORM_BUFFER_H__
#define __SJH_UNIFORM_BUFFER_H__
#include "common/common.h"
#include "GL/gl3w.h"
#include <cstddef>
namespace SJH {
    CLASS_PTR(UniformBuffer)
    /// @brief GL_UNIFORM_BUFFER RAII — 고정크기 할당(DYNAMIC) 후 부분갱신/binding-point 결속.
    class UniformBuffer {
    public:
        static UniformBufferUPtr Create(size_t size);   ///< 실패 시 nullptr
        ~UniformBuffer();
        UniformBuffer(const UniformBuffer&)=delete; UniformBuffer& operator=(const UniformBuffer&)=delete;
        UniformBuffer(UniformBuffer&&)=delete; UniformBuffer& operator=(UniformBuffer&&)=delete;
        void Update(const void* data, size_t bytes, size_t offset=0) const; ///< glBufferSubData
        void BindBase(GLuint bindingPoint) const;                           ///< glBindBufferBase
        GLuint Get() const { return mBuffer; }
        size_t Size() const { return mSize; }
    private:
        UniformBuffer()=default; bool Init(size_t size);
        GLuint mBuffer{0}; size_t mSize{0};
    };
}
#endif
```
`src/program/uniform_buffer.cpp`:
```cpp
#include "program/uniform_buffer.h"
namespace SJH {
    UniformBufferUPtr UniformBuffer::Create(size_t size){ auto u=UniformBufferUPtr(new UniformBuffer()); return u->Init(size)?std::move(u):nullptr; }
    bool UniformBuffer::Init(size_t size){ mSize=size; glGenBuffers(1,&mBuffer); if(!mBuffer) return false;
        glBindBuffer(GL_UNIFORM_BUFFER,mBuffer); glBufferData(GL_UNIFORM_BUFFER,(GLsizeiptr)size,nullptr,GL_DYNAMIC_DRAW); glBindBuffer(GL_UNIFORM_BUFFER,0); return true; }
    UniformBuffer::~UniformBuffer(){ if(mBuffer) glDeleteBuffers(1,&mBuffer); }
    void UniformBuffer::Update(const void* d,size_t b,size_t o) const { glBindBuffer(GL_UNIFORM_BUFFER,mBuffer); glBufferSubData(GL_UNIFORM_BUFFER,(GLintptr)o,(GLsizeiptr)b,d); glBindBuffer(GL_UNIFORM_BUFFER,0); }
    void UniformBuffer::BindBase(GLuint bp) const { glBindBufferBase(GL_UNIFORM_BUFFER,bp,mBuffer); }
}
```
`src/program/CMakeLists.txt` 의 `add_library` 소스 목록에 `uniform_buffer.cpp` 추가(새 link 불요 — 동일 모듈). 검증: `cmake --build --preset ninja --target program` EXIT 0. 커밋 `[engine] : SJH::UniformBuffer RAII`.

### T3 — Program UBO introspection + 소유
**파일:** `src/program/program.h`, `src/program/program.cpp` (CMakeLists 수정 불요 — uniform_buffer 가 동일 모듈이라 새 link 없음).

**(a) `program.h`** — include 에 `#include "program/uniform_buffer.h"` + `<vector>` + `<string>` 추가(★ `buffer/` 아님). class `public:` 에:
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

        /// @brief active uniform block 보유 여부 (Slang UBO 셰이더 식별).
        bool HasUniformBlocks() const { return !mUniformBlocks.empty(); }
        /// @brief 정규화 이름으로 블록 조회 (없으면 nullptr).
        const UniformBlock* FindUniformBlock(const std::string& normalizedName) const;
        /// @brief 모든 블록을 각자의 binding point 에 BindBase (드로우 전 1회).
        void BindUniformBlocks() const;
        /// @brief 정규화 이름 블록의 UBO 에 부분 업로드 (offset/bytes 는 호출자가 std140 으로 계산).
        void UpdateUniformBlock(const std::string& normalizedName, const void* data, size_t bytes, size_t offset) const;
```
class `private:` 에:
```cpp
        std::vector<UniformBlock> mUniformBlocks;   ///< link 후 introspect (비-UBO 셰이더는 빈 벡터)
        void BuildUniformBlocks();                  ///< Create 가 link 직후 호출
```

**(b) `program.cpp`** — 상단 include 에 `#include <cctype>` 추가. `Create()` 의 `mUniformCache.Build(*program);` **직후**:
```cpp
        program->BuildUniformBlocks();   // Phase 2 — Slang UBO 블록 자기기술
```
그리고 `namespace SJH` 안에:
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
검증: `cmake --build --preset ninja --target program` EXIT 0 (RAII static_assert 유지 — `mUniformBlocks` 는 move-only UPtr 멤버라 non-copy/non-move 일관). 커밋 `[engine] : Program UBO 블록 introspection + UniformBuffer 소유`.

### T4 — 렌더 경로 UBO 분기 (비전치 D13)
**파일:** `src/render/mesh_pass_processor.cpp`. WorldMesh 분기 [`:151-177`](../../src/render/mesh_pass_processor.cpp#L151)(결정1 view/proj `:154-157` · 결정2 material `:164` · 결정4 model `:174-175`)를 아래로 **교체**. loose 경로는 비-UBO 셰이더용으로 보존(else).

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
`material->Properties.Vec4s` 접근용 `#include "material/material_property_block.h"` 가 없으면 추가(보통 material.h 경유로 이미 포함). `vmath` 는 함수 인자(viewMat)로 이미 포함. 검증 `cmake --build --preset ninja --target _MyApp_` EXIT 0(불릿 MeshRenderer 비활성이라 런타임 변화 아직 없음). 커밋 `[engine] : 렌더 경로 UBO 분기 (비전치 D13)`.

### T5 — 불릿 PoC + R1 육안 게이트 ★
`apps/_MyApp_/src/Entity/Bullet/bullet_factory.h:133` 의 주석된 `actor->AddComponent<SJH::Scene::MeshRenderer>(mesh, mat);` 해제(변수명/시그니처 파일에서 확인). 빌드 후 **사용자가 실행**:
```bash
cd build_ninja/apps/_MyApp_ && ./_MyApp_
```
**사용자 육안:** 발사한 마젠타 구(불릿)가 ✅정상 위치로 날아가면 → R1 통과(비전치 정답), **T6 SKIP**. ❌사라짐/찌그러짐 → **T6**. 커밋 `[shader] : 불릿 MeshRenderer 활성화 (slang UBO PoC)`.

### T6 (조건부, T5 가 ❌일 때만) — 행렬 전치
T4 의 UBO 행렬 기록을 전치본으로: `vmath::mat4 vT = viewMat.transpose();` 등(`include/vmath.h:845` 멤버) 후 `&vT` 업로드. view/proj/model 모두. 재빌드→재실행→육안 정상 확인. 커밋 `[engine] : R1 확정 — UBO 행렬 전치`. 그리고 spec/plan 의 R1·D13 을 "전치 필요 확정"으로 갱신.

---

## 7. Self-review 체크리스트 (보고 전)
- [ ] `git log game/main..HEAD` — 커밋들이 path-scoped, 사용자 WIP 미포함?
- [ ] `src/program/uniform_buffer.*` 가 buffer 아닌 program 모듈에? (사이클 0)
- [ ] 비-UBO 셰이더(phong 등) loose 경로 보존 — 회귀 없음?
- [ ] R1 은 사용자 육안 보고로만 확정(임의 단정 금지)?
- [ ] 빌드 `--target _MyApp_` EXIT 0?

## 8. Report 형식
DONE / DONE_WITH_CONCERNS / BLOCKED + 변경 파일 목록 + 각 커밋 SHA + R1 육안 결과(✅/❌, T6 수행 여부) + deferred 항목.

## 9. Pointers
- 정본 spec(로컬 gitignore): `doc/superpowers/specs/2026-06-20-slang-shader-migration-design.md` (결정 D1~D13, PoC 로그).
- 상세 plan(로컬 gitignore): `doc/superpowers/plans/2026-06-20-slang-phase2-engine-ubo.md` (6 Task 전체 코드 — 이미 사이클 정정 반영).
- Phase 1 툴체인 핸드오프(원작성자 입력): `~/Downloads/SLANG_TOOLCHAIN_HANDOFF.md`.
- 파킹 브랜치 `game/slang-phase2` (shader split `911ca37`만).

## Change log
- 2026-06-20: 최초 작성. Phase 1 머지 완료(90353f4), Phase 2 설계 완료·코드 0 상태에서 핸드오프. 사이클(buffer→program) 정정 반영 — UniformBuffer 는 src/program/.
