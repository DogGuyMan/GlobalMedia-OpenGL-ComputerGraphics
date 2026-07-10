# 리서치 #4 — 타임라인/시퀀서 재사용 위젯 (S4 미래 정찰)

> ⚠️ **현재 스펙 반영 금지.** 핸드오프 §0·§3·§5: 본 보고서는 **S4 Playable 타임라인 에디터** 사전 정찰용이며, 현재 진행 중인 foundation 스펙/플랜에 **절대 반영하지 않는다.** 보관만 하고 S4 착수 시점에 꺼내 쓴다.

- **날짜**: 2026-06-19
- **상태**: 배치 B (보류) — S4 정찰
- **도구**: Context7 MCP (`/cedricguillemet/imguizmo`) + fallback web(imnodes/ImNodeFlow는 Context7 미보유 → GitHub). ImGuiNeoSequencer는 1차 출처 미확보(아래 E 참조).

---

## A. 확정 사실 (인용 가능)

### A1. ImGuizmo — ImSequencer / ImCurveEdit / GraphEditor 3종 [타임라인 핵심: must-have 후보]
출처: Context7 `/cedricguillemet/imguizmo` (`llms.txt`)

**(1) `ImSequencer::Sequencer` — 멀티 트랙 타임라인**
- 편집 가능한 타임라인. 호출자가 `SequenceInterface`를 서브클래싱 → 프레임 범위(`GetFrameMin/Max`), 아이템 리스트, 아이템 타입(예: Camera/Music/Effect), 선택적 per-item 커스텀 드로잉 제공.
- 지원 동작 플래그(확정): `SEQUENCER_EDIT_STARTEND`, `SEQUENCER_ADD`, `SEQUENCER_DEL`, `SEQUENCER_COPYPASTE`, `SEQUENCER_CHANGE_FRAME`.
- 선택 변경 시 `true` 반환. `currentFrame`/`firstFrame`/`selectedEntry` 상태를 호출자가 보유.
- → **트랙 + 아이템(start/end) + 프레임 스크럽 + 추가/삭제/복붙**이 박스로 제공. AE식 시퀀스 트랙의 뼈대에 직결.

**(2) `ImCurveEdit::Edit` — 이징/키프레임 커브 에디터**
- `Delegate` 서브클래싱 → 커브 수, 포인트 배열(`ImVec2*`), 범위(min/max), 커브 색, **커브 타입(`CurveSmooth` 등)** 제공.
- `EditPoint`(포인트 이동, X정렬 유지), `AddPoint`(포인트 추가) 구현.
- → **이징 커브(스무스 등) + 키프레임 포인트 편집**을 박스로 제공. 타임라인의 값 보간 곡선에 직결.

**(3) `GraphEditor` — 노드 그래프**
- `Delegate` 서브클래싱 → 노드/템플릿/링크, `AllowedLink`(연결 검증), `SelectNode`/`MoveSelectedNodes`, `AddLink`/`DelLink`, `RightClick`, `CustomDraw`(ImDrawList 직접) 구현.
- → 노드 기반 편집이 필요하면 사용.

**의존성/WASM (논리적 귀결)**: ImGuizmo 위젯들은 **Dear ImGui 코어(ImDrawList/ImVec2)에만 의존** → ImGui가 빌드되는 곳이면 **WASM 포함 빌드 가능**. (단 "WASM 공식 지원 명시"는 Context7 미반환 — 의존성 구조에 따른 귀결로 표기.)
**라이선스/ImGui 버전**: Context7 미반환 → **MIT(통설)이나 LICENSE 직접 확인 필요**, 호환 ImGui 버전도 태그에서 확인 필요(불확실).

### A2. imnodes (Nelarius/imnodes) — 의존성 없는 노드 에디터 [common]
출처: web — GitHub Nelarius/imnodes README/releases
- "small, **dependency-free** node editor for dear imgui". **유일 의존 = ImGui 자체.**
- 배포: `imnodes.h` + `imnodes_internal.h` + `imnodes.cpp` **copy-paste** (단일 헤더+소스).
- immediate-mode. **사용자가 모든 상태 보유**(노드/링크/핀 id는 정수). 노드 안에 일반 ImGui 위젯 중첩 가능.
- 기능: 다중 노드/링크 박스 선택, **미니맵**(+hovering 콜백), 핀 모양(circle/quad/triangle, filled/unfilled), 패닝(트랙패드 modifier), 노드 타이틀바 커스텀(`BeginNodeTitleBar`/`EndNodeTitleBar`), `save_load.cpp` 직렬화 예제. ImGui와 동일 C++ 스타일(모던 C++ 미사용).
- **WASM(귀결)**: ImGui만 의존 + 백엔드 비의존(ImGui draw command만 emit) → ImGui가 빌드되는 곳이면 WASM 빌드 가능.
- 라이선스: **MIT(통설) — LICENSE 직접 미확인(불확실).**

### A3. ImNodeFlow (Fattorino/ImNodeFlow) — 블루프린트형 노드 에디터 [optional]
출처: web — GitHub Fattorino/ImNodeFlow README
- "Node based editor/blueprints for ImGui". 커스텀 노드+로직만 정의하면 **연결/에디터 로직/렌더링은 ImNodeFlow가 처리**(imnodes보다 더 "알아서 해주는" 스타일).
- 빌드: CMake `FetchContent`로 ImGui+ImNodeFlow 자동 fetch. 예제 = SDL2+OpenGL3. `imgui_stdlib` 사용. `-DUSE_SYSTEM_IMGUI=ON` 옵션.
- 라이선스: **본 조회 미확인.**

---

## B. 비교표 (S4 정찰용)

| 위젯 | 역할 | 핵심 기능 | 상태보유 | WASM | 라이선스 |
|---|---|---|---|---|---|
| **ImGuizmo ImSequencer** | 멀티트랙 타임라인 | 트랙/아이템 start·end/프레임 스크럽/add·del·copypaste/per-item draw | 호출자 | ✅(귀결) | MIT(미확인) |
| **ImGuizmo ImCurveEdit** | 이징 커브 | 커브 타입(smooth 등)/키포인트 편집·추가/범위 | 호출자 | ✅(귀결) | MIT(미확인) |
| ImGuizmo GraphEditor | 노드 그래프 | 템플릿/링크/검증/커스텀draw | 호출자 | ✅(귀결) | MIT(미확인) |
| imnodes | 노드 에디터 | 미니맵/박스선택/핀모양/직렬화예제 | **호출자(완전)** | ✅(귀결) | MIT(미확인) |
| ImNodeFlow | 블루프린트 | 연결/로직/렌더 자동 처리 | 라이브러리 | ✅(귀결, 추정) | 미확인 |
| ImGuiNeoSequencer | 타임라인 | **미확인** | — | — | **미확인** |

---

## C. 권장 — "재사용 vs 직접 구현" 경계 (의견, 확정 아님)

- **C1. 타임라인+키프레임 코어는 재사용 강력 후보**: AE식 시퀀스 트랙의 뼈대(트랙/아이템/스크럽)는 **ImSequencer**, 값 보간 곡선은 **ImCurveEdit**가 박스로 제공 → S4에서 바닥부터 짜기보다 **이 둘을 차용**하고, AE 특유의 폴리시(스냅/트랙 중첩/마그넷/줌)만 위에 자작이 합리적.
- **C2. 노드 그래프가 필요해지면**: Playable 그래프가 "데이터를 사용자가 소유"하는 스키마 구동 철학과 맞으므로 **imnodes(상태 완전 보유)** 가 정합적. "로직까지 라이브러리가 처리"를 원하면 ImNodeFlow. ImGuizmo GraphEditor는 ImGuizmo를 이미 쓸 경우의 보너스.
- **C3. 차용 방식**: 리서치 #3 C2와 동일 — 번들 통째가 아니라 **위젯 소스만 raw WGPU 빌드에 합치기**(전부 ImGui 코어 의존이라 WGPU 백엔드와 무관하게 동작).
- **C4. 직접 구현이 필요한 부분**: 라이브 스프라이트 sim(WGPU draw)과 타임라인의 결합, AE식 트랙 UX 폴리시는 재사용 위젯이 커버 안 함 → 자작 영역.

---

## D. (참고) S4 착수 시 검증 체크리스트
- 각 위젯 LICENSE 파일 직접 확인(MIT 여부 확정).
- 각 위젯이 요구하는 ImGui 버전 ↔ 에디터가 핀한 신형 ImGui(#1) 호환성.
- ImSequencer/ImCurveEdit를 WGPU 백엔드 + emscripten에서 실제 빌드 1회 검증.
- ImGuiNeoSequencer 별도 조사(E 참조) 후 ImSequencer와 비교.

---

## E. 불확실 / 후속 검증 (거짓 확신 금지)
1. **ImGuiNeoSequencer** — 본 조회에서 **1차 출처 미확보**. 기능/저자/라이선스 모두 **미확인**. S4 착수 시 별도 조사 필요. (추정으로 채우지 않음.)
2. **모든 위젯의 정확한 라이선스** — Context7/web에서 LICENSE 파일을 직접 인용하진 못함. "MIT 통설"은 확정 아님 → 채택 전 확인.
3. **각 위젯 ↔ 신형 ImGui 버전 호환** — 미확인. #1의 ImGui 버전 확정 후 교차 검증 필요.
4. **WASM 빌드 가능성** — "ImGui 코어 의존 → ImGui 빌드되는 곳이면 가능"의 귀결이며, 각 위젯의 emscripten 실증 빌드는 미수행.
5. ImNodeFlow 라이선스/유지보수 활성도 — 미확인.
