# OpenGL / Graphics 노트 — LearnOpenGL 챕터 구조 재편

> 흩어지고 중복되던 6개 노트를 [LearnOpenGL](https://learnopengl.com/) 챕터 구조에 맞춰 재편한 결과.
> `EngineAPI.md` 는 이론 노트가 아닌 자체 엔진 API 레퍼런스라 정리 대상에서 제외했다.

## 구조

```
01_GettingStarted/
  01_OpenGL_상태머신.md          (← GLState.md)
  02_AdvancedData_버퍼분할.md     (← AttributeNBuffer.md 시나리오 1·2·3·5)
  03_Textures_샘플러유닛.md       (← UniformTexture.md)  * 텍스처 핸들/유닛 SSoT

02_Lighting/
  01_BasicLighting_Phong.md      (← Lighting.md Q1~Q4)  * Normal행렬·Specular view의존 SSoT
  02_Materials_LightingMaps.md   (← 멀티플라이팅.md §3)
  03_LightCasters.md             (← 멀티플라이팅.md §1.1·§1.3·§1.4)
  04_MultipleLights.md           (← 멀티플라이팅.md §1.2·§2·§6)

04_AdvancedOpenGL/
  01_DepthTesting.md             (← FrameBuffer.md depth 섹션)
  02_StencilTesting.md           (← FrameBuffer.md §8·§9·§10)
  03_Blending.md                 (← FrameBuffer.md blending·OIT 섹션 + image/)
  05_Framebuffers.md             (← FrameBuffer.md FBO/RBO 섹션 §1~9)
  10_Instancing.md               (← AttributeNBuffer.md 시나리오 4)

05_AdvancedLighting/
  01_BlinnPhong.md               (← Lighting.md Q4 의 Blinn-Phong 소절)
```

## 중복 제거 — 단일 출처(SSoT) 결정

| 개념 | 통합된 단일 위치 | 다른 노트는 |
|---|---|---|
| 텍스처 핸들 vs 유닛 번호 | `03_Textures_샘플러유닛` | 참조 링크만 |
| GL 상태 스코프 (전역/program-local) | `01_OpenGL_상태머신` | 참조 링크만 |
| Normal 행렬 `transpose(inverse(M))` | `01_BasicLighting_Phong` Q1 | 멀티라이팅 §4는 cross-ref |
| Specular view 의존성 | `01_BasicLighting_Phong` Q4 | — |
| Depth test 상태 분류 | depth 본체=`01_DepthTesting`, 일반론=`01_OpenGL_상태머신` | — |

## 제외한 내용 (이론 외)

- **`EngineAPI.md` 전체** — 자체 SJH 엔진 아키텍처/API 레퍼런스.
- **진단(Diagnostics) 모듈** — `멀티플라이팅.md §4·§5`, `GLState.md` 의 `CheckGL*`/`CaptureGLState` 인프라.
- **엔진 plumbing 디버깅 서사** — `FrameBuffer.md` 의 MeshPassProcessor/PassKind/QueueLayer 케이스 스터디. (단, 거기서 *그래픽스 이론 핵심*인 "투명=양면(cull off)"·"depth mask"·"정렬 방향"은 `03_Blending` 으로 흡수.)
- 빌드/크로스플랫폼 설정 섹션.

## 비고

- 원본 노트의 `../../src/...` 코드 경로 링크는 정리 대상 구조에서 해석 불가라 제거하고, 노트 간 cross-reference 만 새 상대경로로 갱신.
- LearnOpenGL 챕터 번호를 폴더/파일 prefix 로 유지 (03_Model-Loading 은 해당 노트 없음 -> 결번).
