# 기말 보고서 3-x 단락 작성 + Parametric 스타일 전환 Implementation Plan

> ⚠ 2026-06 시점 문서 (archival) — 코드 경로는 작성 당시 기준.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `doc/report/20192460_버그잡는게임_홍상준.docx` 의 [9–12]페이지 러프 단락(3-4·3-6·3-7)을 보고서 톤의 짧은 불릿로 완성하고, 3-5 Parametric Surface의 다크테마 스타일을 라이트테마로 전환하되 OMML 수식 23개·코드·이미지·3-8·표는 한 글자도 손상하지 않는다.

**Architecture:** 두 가지 편집 경로를 분리한다. (1) **수식이 없는** 작성 대상(3-4/3-6/3-7)은 MCP `word-document-server` 툴로 안전하게 편집한다. (2) **수식 23개가 밀집한** 3-5 스타일 전환은 MCP를 쓰지 않고 docx(zip)를 풀어 `word/document.xml` 의 배경(`w:shd`)·글자색(`w:color`)·폰트(`w:rFonts`)만 정규식 치환한 뒤 다시 압축한다 — `<m:oMath>` 는 절대 건드리지 않는다. 매 Task 후 수식/이미지/표 개수를 기준선과 대조해 무손상을 증명한다.

**Tech Stack:** MCP `word-document-server`(python-docx 기반), Python 표준 라이브러리(`zipfile`/`shutil`/`re`), Bash. 자료: `doc/report/{RenderPipeline,Healthbar,PostProcessing,ShaderSkybox}.md`.

---

## 배경 — 현재 문서 상태 (2026-06-14 03:58 / rev16 / 150단락 기준)

편집 대상 파일: `doc/report/20192460_버그잡는게임_홍상준.docx` (단일 파일)

| 단락(현 index) | 제목/상태 | 처리 | 자료 |
|---|---|---|---|
| 73 | `3-4. [ SceneGraph + Actor & Compoenet ]` + `[핵심 전달]` + 러프 불릿 3개(78·79·80, `pStyle a4` numId5, 비bold) | **작성** | `src/scene`(actor) + `src/object`(transform) |
| 82~131 | `3-5. [ Parametric Surface ]` 본문 완성(개념 ①②, URL 3, 코드블록, **OMML 수식 23개**), 다크테마 | **스타일 전환** | 이미 작성됨 |
| 133~136 | `3-6. [ Renderer & Multi Pass Rendering ]` + 러프 불릿 3개(134·135·136, `pStyle a4` numId10, bold) | **작성** | `RenderPipeline.md` |
| 137~143 | `3-7. [Shader VFX & Postprocess &  Skybox + 매트릭스 효과]` + 러프 불릿 6개(138~143, `pStyle a4` numId11, bold) | **작성** | `Healthbar.md`+`PostProcessing.md`+`ShaderSkybox.md` |
| 144~145 | `3-8. [ Adaptive Audio System & FMOD ]` + `[내가 작성할 예정]` | 🔒 **보존** | — |

> ⚠️ 단락 index 는 편집 중 바뀐다. 모든 도구 호출은 **헤더/불릿 텍스트로 앵커**하고, index 는 매 Task 직전 `get_document_outline` 으로 재확인한다.

## 보존 불변식 (INVARIANTS) — 매 Task 후 반드시 재측정

작업 전후로 아래 수치/요소가 **변하면 안 된다**. Task 0 에서 기준선을 파일로 박제하고, 매 Task 종료 스텝에서 대조한다.

- `<m:oMath>` = **23**, `<m:oMathPara>` = **4** (전부 3-5 Parametric 안)
- `<w:drawing>` = **13** (이미지: rId5~18 = 3-1~3-3 영역 11개 + 그래픽9 `rId19`/`rId20` + 그래픽10 `rId21`/`rId22` = 3-4 직후 2개)
- `<w:tbl>` = **2** (상단 메타 표 + "4. AI 사용 내역" 표)
- 하이퍼링크 `rId23`(youtube)·`rId24`(mobius.c)·`rId25`(heart.cpp) 3개 (3-5)
- `[내가 작성할 예정]` 단락(3-8) 텍스트 불변
- 3-1·3-2·3-3 ([4–7]페이지) 텍스트 불변

## 도구 전략 — 무엇으로 어떻게 편집하나

- **3-4/3-6/3-7 (MCP 안전 경로)**: 수식·이미지 없는 단락만 다룬다.
  - 헤더 아래 블록 교체가 가능한 3-6/3-7 은 `replace_paragraph_block_below_header(header_text, new_content)` 후보. **단 다음 헤더 전까지만 교체되는지** 프로브로 검증한다(아래 Task 2 Step 1).
  - 3-4 는 헤더 바로 아래 **이미지 그래픽9/10이 있어** 블록 교체 금지. `[핵심 전달]` 아래 러프 불릿(78·79·80)만 개별 `search_and_replace` 로 교체한다.
  - 폴백: 블록 교체가 인접 헤더/이미지를 침범하면 → 러프 불릿 단락별 `search_and_replace`(텍스트 일부 매칭) 사용.
- **3-5 (XML 직접 경로)**: MCP 미사용. docx 백업 → unzip → `word/document.xml` 정규식 치환(배경/색/폰트) → rezip. `<m:oMath …>…</m:oMath>` 블록은 정규식 대상에서 제외(수식 안의 색/폰트도 그대로 둔다).
- **검증**: 매 편집 후 `get_document_outline`/`get_document_text` 재읽기 + zip 내 `document.xml` 에서 불변식 grep 카운트.

---

## File Structure

- **편집**: `doc/report/20192460_버그잡는게임_홍상준.docx` (단 하나의 산출물)
- **생성(백업)**: `doc/report/20192460_버그잡는게임_홍상준.BACKUP-2026-06-14.docx`
- **생성(기준선)**: `/tmp/report_invariants_baseline.txt` (수식/이미지/표 카운트 박제)
- **참조(읽기 전용)**: `<doc>/report/RenderPipeline.md`, `Healthbar.md`, `PostProcessing.md`, `ShaderSkybox.md`, `src/object/transform.h`, `src/scene/{actor.h,actor.cpp,compound_actor.h}`

---

## Task 0: 백업 + 불변식 기준선 박제

**Files:**
- Create: `doc/report/20192460_버그잡는게임_홍상준.BACKUP-2026-06-14.docx`
- Create: `/tmp/report_invariants_baseline.txt`

- [ ] **Step 1: 원본 백업**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
cp "20192460_버그잡는게임_홍상준.docx" "20192460_버그잡는게임_홍상준.BACKUP-2026-06-14.docx"
ls -la *.docx
```
Expected: 원본 + BACKUP 두 파일 존재, 동일 바이트 크기.

- [ ] **Step 2: 불변식 기준선 측정 (zip 내부 document.xml 직접 카운트)**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY' | tee /tmp/report_invariants_baseline.txt
import zipfile, re
xml = zipfile.ZipFile("20192460_버그잡는게임_홍상준.docx").read("word/document.xml").decode("utf-8")
print("oMath",      len(re.findall(r"<m:oMath[ >]", xml)))
print("oMathPara",  len(re.findall(r"<m:oMathPara[ >]", xml)))
print("drawing",    len(re.findall(r"<w:drawing[ >]", xml)))
print("blip",       len(re.findall(r"<a:blip[ >]", xml)))
print("svgBlip",    len(re.findall(r"svgBlip", xml)))
print("tbl",        len(re.findall(r"<w:tbl[ >]", xml)))
print("내가작성할예정", xml.count("내가 작성할 예정"))
PY
```
Expected (이 값이 이후 모든 Task의 비교 기준):
```
oMath 23
oMathPara 4
drawing 13
blip 13
svgBlip 5
tbl 2
내가작성할예정 1
```
만약 23/4/13/2 와 다르면 → 문서가 또 바뀐 것. **중단하고 `get_document_outline` 으로 현황 재조사 후 plan 갱신.**

- [ ] **Step 3: 커밋 (working tree 격리 — partial add)**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git add "doc/report/20192460_버그잡는게임_홍상준.BACKUP-2026-06-14.docx"
git commit "doc/report/20192460_버그잡는게임_홍상준.BACKUP-2026-06-14.docx" -m "[report] 편집 전 백업 스냅샷"
```
> 사용자가 같은 working tree 에서 병렬 작업 중일 수 있으므로 **인덱스 전체 커밋 금지**, 경로 지정 partial 커밋만.

---

## Task 1: 3-4 [SceneGraph + Actor & Component] 작성

**Files:**
- Modify: `doc/report/20192460_버그잡는게임_홍상준.docx` (`[핵심 전달]`(77)→대표 모듈 라인, 러프 불릿 78·79·80)
- Read(확인 완료): `src/object/transform.h`, `src/scene/actor.h`, `src/scene/actor.cpp`

**보존:** 헤더(73) 바로 아래 이미지 그래픽9(`rId19/20`)·그래픽10(`rId21/22`) — **절대 블록 교체 금지.** 개별 단락만 search_and_replace.

**코드 근거(확인 완료):**
- `Transform`(transform.h): 로컬 TRS(Translate·EulerRot·Scale) 값 객체, `GetLocalMatrix()`=T·R·S. 계층/월드 합성은 비-책임(Actor 담당).
- `Actor::GetWorldMatrix()`(actor.cpp:97-106): `mParent->GetWorldMatrix() * local` — 부모(mParent) 체인을 루트까지 재귀로 곱함.
- Actor 비상속(actor.h:12-14, 95-97): `class FooActor : public Actor` 금지, 특수 속성=Component(OnEnter/OnExit/Update) 부착, `AddComponent<T>`/`GetComponent<T>`(Unity 정통).
- `compound_actor.h` free factory(`CreateCameraActor` 등).

- [ ] **Step 1: 현재 3-4 러프 불릿 텍스트/구조 확인**

```
find_text_in_document(text_to_find="CompoundActor")
get_paragraph_text_from_document(paragraph_index=78)
get_paragraph_text_from_document(paragraph_index=79)
get_paragraph_text_from_document(paragraph_index=80)
```
Expected: 78=`SJH Transform API를 사용해…[3줄]`, 79=`상속 구조가 아닌 액터 + 컴포넌트…`, 80=`CompoundActor  및 복합 액터…Json으로 Data Driven…`. (index 가 다르면 `get_document_outline` 으로 재매핑.)

- [ ] **Step 2: `[핵심 전달]`(77)을 대표 모듈 라인으로 교체**

`search_and_replace("[핵심 전달]", "대표 모듈 : src/scene (actor), src/object (transform)")`
> 3-1~3-3 형식 통일(사용자 확정). 교체가 단락을 깨면 78 앞에 `insert_paragraph_near_text` 로 대표 모듈 라인 삽입.

- [ ] **Step 3: 러프 불릿 3개를 완성 불릿로 교체 (개별 search_and_replace)**

각 러프 단락을 아래 완성 문장으로 치환(run 분할로 실패하면 안정적 부분 문자열로 분할 치환). 러프 1번(Transform)은 사용자 지시대로 **한 불릿**으로.

불릿 78 (Transform 체이닝, 한 불릿) →
```
Transform(src/object)은 로컬 TRS(Translate·EulerRot·Scale) 값만 갖고 GetLocalMatrix()로 로컬 행렬을 만든다. 계층·월드 합성은 Actor가 맡아, GetWorldMatrix()가 부모(mParent) 월드 행렬에 자기 로컬을 곱하는 재귀로 루트까지 행렬을 체이닝한다 — 부모가 움직이면 자식이 따라온다.
```
불릿 79 →
```
Actor는 상속을 금지(class FooActor : public Actor 금지)하고, 특수 속성은 Component(OnEnter/OnExit/Update 생명주기)를 부착해 부여한다. AddComponent<T>/GetComponent<T>(Unity GetComponent 정통)로 다뤄 클래스 폭발 없이 유연하다.
```
불릿 80 →
```
자주 쓰는 Actor+Component 조합은 compound_actor.h의 free factory(CreateCameraActor 등)로 묶어 호출부를 간결히 했다. 향후 ImGUI 엔진 에디터와 JSON 직렬화로 씬 구성을 코드와 분리(Data-Driven)할 계획이다.
```
> 톤: 기존 3-1~3-3 개조식. `[핵심 전달]` 라벨은 Step 2에서 대표 모듈로 교체됨. 3불릿 유지(과확장 금지).

- [ ] **Step 4: 검증 — 텍스트 반영 + 불변식 불변**

```
get_paragraph_text_from_document(paragraph_index=78)   # 완성 문장 확인
```
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY'
import zipfile, re
xml = zipfile.ZipFile("20192460_버그잡는게임_홍상준.docx").read("word/document.xml").decode("utf-8")
print("drawing", len(re.findall(r"<w:drawing[ >]", xml)), "oMath", len(re.findall(r"<m:oMath[ >]", xml)))
PY
```
Expected: drawing **13**, oMath **23** (불변). 이미지/수식 손상 0.

- [ ] **Step 5: 커밋**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git commit "doc/report/20192460_버그잡는게임_홍상준.docx" -m "[report] 3-4 SceneGraph+Actor&Component 본문 작성"
```

---

## Task 2: 3-6 [Renderer & Multi Pass Rendering] 작성

**Files:**
- Modify: `doc/report/20192460_버그잡는게임_홍상준.docx`
- Read: `<doc>/report/RenderPipeline.md` (쉬운 설명판 §1~§6)

**보존:** 3-6 영역엔 이미지·수식 없음 → MCP 안전. 단 3-7 헤더를 침범하지 않을 것.

- [ ] **Step 1: 블록 교체 도구 동작 프로브**

```
find_text_in_document(text_to_find="3-6. [ Renderer & Multi Pass Rendering ]")
get_paragraph_text_from_document(paragraph_index=134)
get_paragraph_text_from_document(paragraph_index=137)   # 다음 헤더 3-7 위치 확인
```
Expected: 134~136 이 3-6 러프 불릿, 137 이 `3-7. [Shader VFX…` 헤더. `replace_paragraph_block_below_header` 가 137 전까지만 교체하는지 판단(불확실하면 Step 2 의 개별 치환 폴백 사용).

- [ ] **Step 2: 대표 모듈 라인 추가 + 러프 불릿 3개 교체**

먼저 헤더 `3-6. [ Renderer & Multi Pass Rendering ]` 직후에 `대표 모듈 : src/render (scene_renderer, mesh_pass_processor)` 를 `insert_paragraph_near_text`(앵커=헤더) 로 추가(3-1~3-3 형식 통일). 이어 각 러프 단락(134·135·136)을 `search_and_replace` 로 아래로 치환(블록 교체가 안전하면 한 번에, 아니면 개별).

```
전제: 카메라마다 앞에 렌더 타겟(FBO=텍스처)이 한 장 붙어 있고, 카메라가 그리는 모든 것은 화면이 아니라 그 텍스처에 먼저 그려진다(Unity Camera.targetTexture 정통).
```
```
멀티패스는 Application이 든 stage 벡터를 순서대로 실행하는 것이다 — worldCam → Effekseer → screenCam(후처리) → 최종 화면 출력.
```
```
그리기 직전, 잘 안 바뀌는 데이터(조명)는 패스당 1회, 자주 바뀌는 것(모델 행렬)은 메시마다 셰이더로 보내 GL 상태 전환을 최소화한다.
```
추가 불릿 2개를 `insert_numbered_list_near_text`(앵커=직전 불릿) 또는 `insert_paragraph_near_text` 로 삽입(numId10 불릿 스타일 상속 확인):
```
후처리는 "카메라 앞 사각형에 직전 결과 텍스처를 붙여 다시 찍기"이며, PassComponent의 InputFB·OutputFB 포인터 공유로 패스당 FBO 1개씩 잇는 선형 체인이다.
```
```
외부 모듈도 같은 그림판 체계로 흡수했다 — Effekseer는 stage로 위장(ParticleStage)해 sceneFB에 합성하고, ImGui는 모든 패스가 끝난 뒤 백버퍼에 직접 얹는다.
```
> 삽입 도구가 numPr 불릿 스타일을 못 살리면(평문으로 들어가면) → 추가 불릿을 포기하고 기존 3불릿만 충실히 채운다(과확장 금지). "대표 모듈 : src/render" 라인을 헤더 직후에 둘지는 3-1~3-3 대칭 위해 권장하나 선택.

- [ ] **Step 3: 검증**

```
get_document_text  # 3-6 영역 육안 확인, 3-7 헤더 보존 확인
```
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 -c "import zipfile,re; x=zipfile.ZipFile('20192460_버그잡는게임_홍상준.docx').read('word/document.xml').decode(); print('drawing',len(re.findall(r'<w:drawing[ >]',x)),'oMath',len(re.findall(r'<m:oMath[ >]',x)),'tbl',len(re.findall(r'<w:tbl[ >]',x)))"
```
Expected: drawing 13, oMath 23, tbl 2 (불변). 3-7 헤더 텍스트 잔존.

- [ ] **Step 4: 커밋**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git commit "doc/report/20192460_버그잡는게임_홍상준.docx" -m "[report] 3-6 Renderer & Multi Pass Rendering 본문 작성"
```

---

## Task 3: 3-7 [Shader VFX & Postprocess & Skybox + 매트릭스] 작성

**Files:**
- Modify: `doc/report/20192460_버그잡는게임_홍상준.docx`
- Read: `<doc>/report/Healthbar.md`, `PostProcessing.md`, `ShaderSkybox.md`

**보존:** 3-7 영역엔 이미지·수식 없음 → MCP 안전. 3-8 `[내가 작성할 예정]` 헤더/문구 절대 침범 금지.

- [ ] **Step 1: 현재 3-7 러프 불릿 6개 확인 + 3-8 경계 확인**

```
find_text_in_document(text_to_find="SkyBox매트릭스 Rain 효과")   # 138~143 위치
find_text_in_document(text_to_find="내가 작성할 예정")            # 3-8 경계
```
Expected: 138=Fog, 139=GrayScale, 140=Vignette, 141=Bloom, 142=HealthBar, 143=SkyBox매트릭스. 145=`[내가 작성할 예정]`.

- [ ] **Step 2: 대표 모듈 라인 추가 + 러프 불릿 6개 교체 (각각 search_and_replace)**

먼저 헤더 `3-7. [Shader VFX...` 직후에 `대표 모듈 : apps/_MyApp_/resources/shaders (postprocess, healthbar, matrix_skybox)` 를 `insert_paragraph_near_text` 로 추가(3-1~3-3 형식 통일). 이어 각 러프 불릿을 치환:

| 러프(현재) | 완성 문장 |
|---|---|
| `Fog를 어떻게 Depth Map을 가져왔는지` | `Fog: 깊이 맵(uDepth)의 비선형 NDC 깊이를 역투영(uInverseProjection)해 카메라-픽셀 실제 거리를 복원하고, 그 거리로 안개색을 mix한다(far plane 하늘은 제외).` |
| `GrayScale :` | `GrayScale: 휘도 dot(rgb,(0.299,0.587,0.114))로 무채색을 만들어 원본과 mix한다.` |
| `Vignetee : 가우시안과 마스킹 방법` | `Vignette: 화면 중심거리 d로 가우시안 마스크 exp(-k·d²)를 만들어 중앙=장면·외곽=비네트색으로 mix한다(둥근 원형 스텐실).` |
| `Bloom : 효과` | `Bloom: 휘도로 밝은 픽셀만 step으로 골라 9×9 박스 블러로 번지게 한 뒤 원본에 더한다(additive glow).` |
| `HealthBar 구현` | `HealthBar: 가로 좌표 u를 fract(u·N)으로 N조각으로 쪼개고, smoothstep과 fwidth로 조각 틈·채움 경계를 줌과 무관하게 1픽셀 폭으로 매끈하게(안티에일리어싱) 그린다.` |
| `SkyBox매트릭스 Rain 효과` | `Skybox 매트릭스 Rain: 방향벡터를 구면좌표(θ=atan2(z,x), φ=asin y)로 역매핑해 UV(θ/2π, φ/π)를 얻고, floor/fract로 글자 격자를 잘라 칸을 채운다.` |

> 맨 앞에 공통 불릿 1개를 `insert_*_near_text`(앵커=3-7 헤더) 로 추가 권장(numId11 스타일 상속되면): `후처리 공통: 장면을 텍스처(uScene)에 그려두고 화면 전체 사각형에 다시 칠하며 픽셀별로 색을 가공하는 풀스크린 패스다. 여러 효과를 패스 체인으로 통과시킨다.` — 스타일 상속 실패 시 생략(6불릿이면 Q4 범위 충족).

- [ ] **Step 3: 검증**

```
get_document_text   # 3-7 6불릿 반영 + 3-8 [내가 작성할 예정] 잔존 확인
```
```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 -c "import zipfile,re; x=zipfile.ZipFile('20192460_버그잡는게임_홍상준.docx').read('word/document.xml').decode(); print('drawing',len(re.findall(r'<w:drawing[ >]',x)),'oMath',len(re.findall(r'<m:oMath[ >]',x)),'내가작성할예정',x.count('내가 작성할 예정'))"
```
Expected: drawing 13, oMath 23, 내가작성할예정 1 (불변).

- [ ] **Step 4: 커밋**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git commit "doc/report/20192460_버그잡는게임_홍상준.docx" -m "[report] 3-7 Shader VFX·Postprocess·Skybox 본문 작성"
```

---

## Task 4: 3-5 [Parametric Surface] 다크→라이트 스타일 전환 (XML 직접)

**Files:**
- Modify: `doc/report/20192460_버그잡는게임_홍상준.docx` (`word/document.xml` 의 shd/color/rFonts만)

**절대 보존:** `<m:oMath>…</m:oMath>` 23개, 코드 텍스트, 이미지, URL. **MCP 미사용.**

- [ ] **Step 1: 치환 대상 패턴을 실제 XML에서 확인 (정확도 검증)**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY'
import zipfile, re
x = zipfile.ZipFile("20192460_버그잡는게임_홍상준.docx").read("word/document.xml").decode("utf-8")
for pat in ['fill="272B33"','fill="27292C"','fill="393F4A"','val="FFFFFF"','val="EDEDED"','val="FF73FD"','val="96CBFE"','val="CFCB90"','val="FFD2A7"','val="AAB1BF"','Helvetica Neue','Menlo','val="56B6C2"','w:val="36"','w:val="30"','w:val="24"','w:val="22"','w:val="20"']:
    print(f"{pat:18} -> {len(re.findall(re.escape(pat), x))}")
# 소제목 크기 정규화 대비: Heading3/4/5 sz=36/30/24(18/15/12pt). 이 값이 Parametric 외(타이틀 28/40·표 21)에
# 안 나오면 전역 치환 안전, 나오면 Step2에서 Parametric 영역 슬라이스 한정. 목표=본문 bold 수준 sz22(11pt).
# 수식 안에 색/폰트가 섞여 있는지(있다면 제외 처리 필요)
omath = re.findall(r"<m:oMath\b.*?</m:oMath>", x, re.S)
joined = "".join(omath)
print("oMath blocks:", len(omath), "| FFFFFF in oMath:", joined.count('val="FFFFFF"'), "| Helvetica in oMath:", joined.count('Helvetica Neue'))
PY
```
Expected: 각 다크 색/배경 패턴이 1개 이상, Menlo 다수. `oMath blocks: 23`. **`FFFFFF in oMath`/`Helvetica in oMath` 가 0이면** 수식과 스타일 치환이 겹치지 않아 안전. 0이 아니면 → Step 2에서 oMath 블록을 마스킹 후 치환.

- [ ] **Step 2: document.xml 치환 스크립트 작성 + 적용 (oMath 블록 보호)**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY'
import zipfile, re, shutil, os
SRC = "20192460_버그잡는게임_홍상준.docx"
TMP = "_restyle_tmp.docx"
shutil.copy(SRC, TMP)

zin = zipfile.ZipFile(TMP, "r")
xml = zin.read("word/document.xml").decode("utf-8")

# 1) oMath 블록을 플레이스홀더로 잠가 치환에서 제외 (수식 1바이트도 불변 보장)
omaths = re.findall(r"<m:oMath\b.*?</m:oMath>", xml, re.S)
for i, blk in enumerate(omaths):
    xml = xml.replace(blk, f"@@OMATH{i}@@", 1)

# 2) 다크 -> 라이트 치환 (배경/글자색/narrative 폰트). 코드 폰트 Menlo는 유지.
repl = {
    'w:fill="272B33"': 'w:fill="FFFFFF"',   # narrative 다크 배경 -> 흰색
    'w:fill="27292C"': 'w:fill="F6F8FA"',   # 코드블록 배경 -> 밝은 회색
    'w:fill="393F4A"': 'w:fill="EFF1F3"',   # 인라인 코드칩 -> 밝은 회색
    'w:val="FFFFFF"': 'w:val="24292E"',     # 흰 글씨 -> 어두운 텍스트
    'w:val="EDEDED"': 'w:val="24292E"',     # 코드 토큰색들 -> 어두운 단색
    'w:val="FF73FD"': 'w:val="24292E"',
    'w:val="96CBFE"': 'w:val="24292E"',
    'w:val="CFCB90"': 'w:val="24292E"',
    'w:val="FFD2A7"': 'w:val="24292E"',
    'w:val="AAB1BF"': 'w:val="24292E"',     # narrative 본문 밝은 회색 -> 어두움
}
for a, b in repl.items():
    xml = xml.replace(a, b)
# narrative 라틴 폰트 Helvetica Neue 제거(본문 폰트 상속). Menlo(코드)는 유지.
xml = re.sub(r'<w:rFonts w:ascii="Helvetica Neue" w:hAnsi="Helvetica Neue"\s*/>', '', xml)

# 2b) 소제목 크기 정규화: Heading3/4/5 sz 36/30/24 -> 본문 bold 수준 22(11pt). 코드 sz20 은 유지.
#     Step1 확인 결과 36/30/24 가 Parametric 외에도 나오면 아래를 seg=xml[i0:i1] 슬라이스 한정으로 바꿀 것.
for a, b in {'w:val="36"': 'w:val="22"', 'w:val="30"': 'w:val="22"', 'w:val="24"': 'w:val="22"'}.items():
    xml = xml.replace(a, b)

# 3) oMath 블록 원복
for i, blk in enumerate(omaths):
    xml = xml.replace(f"@@OMATH{i}@@", blk, 1)

# 4) 새 zip 으로 재패키징 (document.xml만 교체, 나머지 그대로 복사)
zout = zipfile.ZipFile(SRC + ".new", "w", zipfile.ZIP_DEFLATED)
for item in zin.infolist():
    data = xml.encode("utf-8") if item.filename == "word/document.xml" else zin.read(item.filename)
    zout.writestr(item, data)
zout.close(); zin.close()
os.replace(SRC + ".new", SRC)
os.remove(TMP)
print("restyle applied")
PY
```
> URL 링크색 `56B6C2`(청록)는 밝은 배경에서도 보이므로 유지. 코드 토큰 하이라이팅은 단색(`24292E`)으로 합쳐 보고서 톤에 맞춘다(사용자가 "배경/색/폰트 전환" 선택). 코드 텍스트·줄바꿈·들여쓰기는 불변.

- [ ] **Step 3: 무손상 검증 — 수식·이미지·코드 텍스트 보존 증명**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY'
import zipfile, re
x = zipfile.ZipFile("20192460_버그잡는게임_홍상준.docx").read("word/document.xml").decode("utf-8")
print("oMath", len(re.findall(r"<m:oMath[ >]", x)), "(expect 23)")
print("oMathPara", len(re.findall(r"<m:oMathPara[ >]", x)), "(expect 4)")
print("drawing", len(re.findall(r"<w:drawing[ >]", x)), "(expect 13)")
print("tbl", len(re.findall(r"<w:tbl[ >]", x)), "(expect 2)")
print("Menlo kept", "Menlo" in x)
print("code text kept", "SurfaceFunction(u + epsU, v)" in x, "size_t uCount = uRes + 1;" in x)
print("dark leftovers", x.count('272B33'), x.count('27292C'), x.count('FFFFFF'))  # 0,0,0 목표
print("heading sz lowered", x.count('w:val="36"'), x.count('w:val="30"'))  # 0,0 목표(소제목 크기 정규화)
PY
```
Expected: oMath **23**, oMathPara **4**, drawing **13**, tbl **2**, Menlo kept **True**, code text kept **True True**, dark leftovers **0 0 0**.
- 하나라도 어긋나면 → `cp BACKUP… 원본` 으로 **즉시 롤백**하고 치환 규칙 재점검.

- [ ] **Step 4: 문서 열기 검증 (python-docx 파싱 무결성)**

```
get_document_outline   # 3-5 영역이 정상 파싱되는지(예외 없이), 수식 자리 텍스트 유지 확인
```
Expected: 정상 반환(파일 깨짐 없음), 3-5 단락들 잔존. python-docx 가 못 열면 docx 손상 → 롤백.

- [ ] **Step 5: 커밋**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics
git commit "doc/report/20192460_버그잡는게임_홍상준.docx" -m "[report] 3-5 Parametric Surface 다크->라이트 스타일 전환(수식·코드 보존)"
```

---

## Task 5: 최종 통합 검증

**Files:** 없음(읽기/검증만)

- [ ] **Step 1: 불변식 최종 대조**

```bash
cd /Users/escatrgot/DevelopProjects/SSU/GlobalMedia-OpenGL-ComputerGraphics/doc/report
python3 - <<'PY'
import zipfile, re
x = zipfile.ZipFile("20192460_버그잡는게임_홍상준.docx").read("word/document.xml").decode("utf-8")
base = dict(l.split() for l in open("/tmp/report_invariants_baseline.txt"))
now = {
 "oMath": len(re.findall(r"<m:oMath[ >]", x)),
 "oMathPara": len(re.findall(r"<m:oMathPara[ >]", x)),
 "drawing": len(re.findall(r"<w:drawing[ >]", x)),
 "tbl": len(re.findall(r"<w:tbl[ >]", x)),
}
for k in now:
    ok = str(now[k]) == base.get(k)
    print(f"{k}: {now[k]} vs baseline {base.get(k)} -> {'OK' if ok else 'MISMATCH'}")
PY
```
Expected: 4개 항목 모두 `OK`.

- [ ] **Step 2: 보존 대상 텍스트 잔존 확인**

```
get_document_text
```
육안 확인 체크리스트:
- 3-1·3-2·3-3 본문 그대로
- 3-4/3-6/3-7 완성 불릿 반영
- 3-5 Parametric 본문·코드·수식 자리 그대로(스타일만 라이트)
- 3-8 `[내가 작성할 예정]` 그대로
- "4. AI 사용 내역" 표 그대로

- [ ] **Step 3: 사용자 육안 검증 요청 (Word 열기)**

문서를 Word/뷰어로 열어 ① 3-5 라이트테마 가독성(코드/수식 보임) ② 3-4/3-6/3-7 불릿 톤 ③ 이미지 2장 정상 표시 를 사용자가 확인하도록 보고한다. 수식은 Word 렌더링으로만 최종 확인 가능.

---

## Self-Review (작성자 체크)

**1. Spec coverage**
- 요청1(스타일 분석, 3-5 제외): 사전 조사로 완료 → Task 4가 라이트 전환에 그 스타일 데이터 활용 ✅
- 요청2([4–7] 맥락 분석): 사전 분석 완료(분리·응집/엔진 벤치마킹/수학 기반/재사용) → 작성 톤 기준으로 Task 1~3에 반영 ✅
- 요청3([9–12] 내용 추가): Task 1(3-4)/2(3-6)/3(3-7) ✅
- 요청3-1(자료 매핑): 3-6=RenderPipeline, 3-7=Healthbar+PostProcessing+ShaderSkybox ✅
- 요청3-2(자료 미충족 단락 Sub-Agent): 3-8 은 사용자 결정으로 "직접 작성"→보존, 3-4 는 코드베이스 경량 확인 → 별도 Sub-Agent 불요로 축소 ✅(사용자 Q1 답변 반영)
- 요청3-3(보존: [작성예정]·이미지·SVG / [9–12]만 편집): Task 0 백업 + 매 Task 불변식 검증 + 3-8/표/3-1~3-3 비편집 ✅

**2. Placeholder scan:** 각 단락 완성 문장은 실제 텍스트로 기재(TBD 없음). 단 MCP 삽입 도구의 numPr 상속 여부는 런타임 확인 항목으로 명시(폴백 포함) ✅

**3. Type consistency:** 불변식 수치(oMath 23 / drawing 13 / tbl 2 / oMathPara 4)를 전 Task에서 동일 사용 ✅. rId(19~25) 표기 일관 ✅

**위험·완화:**
- (R1) MCP 삽입이 불릿 스타일을 못 살림 → 추가 불릿 포기, 기존 불릿 충실화로 폴백.
- (R2) 3-5 치환이 수식/코드 손상 → oMath 블록 마스킹 + 전후 카운트 + 백업 롤백.
- (R3) 사용자 병렬 편집으로 index/문서 변동 → 매 Task 헤더 텍스트 앵커 + outline 재확인.
