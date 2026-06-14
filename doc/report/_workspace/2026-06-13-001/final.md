## 3-3. SpriteAtlas + UV Spliting Shader

공통점부터 정리하면, sprite와 bitmap font, skybox는 모두 텍스처를 균일한 격자로 나눈다는 점에서 같다. [0, 1] 구간을 col, row 단위로 쪼개는 방식도 동일하다. 여러 그림이나 글자를 cols × rows의 균일한 칸으로 하나의 텍스처에 배치하고, 상수 값만으로 쉽게 조작할 수 있도록 격자 정보를 별도 파일로 빼냈다.

격자는 `UniformAtlas.SetGrid(cols,rows)` 함수로 설정한다. BitmapFont의 경우 xml 형식에 맞춰 `cols = scaleW / cellW`로 같은 격자를 인식하도록 구현했는데, 이 부분은 추후 인터페이스로 분리할 대상이다. 칸 번호는 좌하단에서 좌상단까지 nxm에 맞도록 인덱스를 매핑한다.

각 칸의 시작점과 크기는 uUvRect(uMin,vMin,uSize,vSize)로 만든다. sprite는 `atlas->GetUVRect(frameIdx)`로, 글자는 `FrameOf(cp)`가 `frame = row*cols + col`로 칸 번호를 부여한다.

Phase 3에서는 0~1 좌표를 해당 칸으로 옮긴다(Affine Remap = 공통 식). 정점 셰이더가 quad의 0~1 지역좌표를 그 칸 영역으로 확대·평행이동하는데, 식은 다음과 같다.

`vUv = uUvRect.xy + aTexCoord * uUvRect.zw` = 시작점 + 지역좌표 × 칸크기

skybox도 이 값을 floor/fract로 즉석에서 계산할 뿐, 식 자체는 동일하다.

Phase 4에서는 그 칸만 끌어와 그린다(Gather Sample). 프래그먼트가 vUv로 텍스처를 읽어(gather), 그 칸의 픽셀만 화면에 칠한다(`texture(uAtlas, vUv)`). 결국 숫자 4개(uUvRect)만 바꾸면 같은 텍스처에서 다른 칸(애니 프레임이나 글자)으로 교체된다.

한 줄로 정리하면, 텍스처를 격자로 쪼갠 뒤 칸 번호를 사각형으로 바꾸고(uUvRect), 시작점 + 지역좌표 × 칸크기로 remap한 다음, 그 칸만 gather하는 흐름이다. 차이는 칸 선택을 CPU가 미리 하느냐(sprite·글자), 아니면 셰이더가 floor/fract로 즉석에서 하느냐(skybox)에 있을 뿐이다.

<!-- HUMANIZE-SUMMARY v1.6.1
run_id: 2026-06-13-001
metrics:
  char_in: 689
  char_out: 712
  change_rate: 18.5%
  self_check: 6/6
  grade: A
categories:  # before → after
  E-2 명사형 종결 혼용(뺌·매핑함·구현 등): 6 → 0
  E-1 단편적 리듬(끊긴 메모체): 7 → 0
  A-2 '~단위로 나눔' 등 어색한 명사구 직결: 3 → 0
  D-7 'X에서 Y로' 변환 반복: 1 → 1 (내용상 보존)
self_check:
  - 고유명사·수치·인용·코드·식 100% 보존: ✅ (SpriteAtlas, uUvRect, GLSL식, 함수명, Phase 3/4 전부 원형)
  - 변경률 30% 이하: ✅ (18.5%)
  - 장르 이탈 없음: ✅ (리포트 산문 유지)
  - register 보존: ✅ (메모형 명사종결을 리포트 평서격식체로 정규화, 다운그레이드 아님)
  - S1 잔존 0건: ✅
  - 인공 표현 추가 없음: ✅ (비유·수사 미추가)
highlights:
  - id: E-2
    before: "상수 값으로 쉽게 조작하도록 파일을 빼냄."
    after: "상수 값만으로 쉽게 조작할 수 있도록 격자 정보를 별도 파일로 빼냈다."
  - id: E-1
    before: "그 칸의 시작점·크기 = uUvRect(...)를 만든다. → atlas->GetUVRect(frameIdx), 글자는 FrameOf(cp) 가 frame = row*cols + col 로 번호를 준다."
    after: "각 칸의 시작점과 크기는 uUvRect(...)로 만든다. sprite는 atlas->GetUVRect(frameIdx)로, 글자는 FrameOf(cp)가 frame = row*cols + col로 칸 번호를 부여한다."
  - id: E-2
    before: "칸 번호를 좌하단부터, 좌상단까지 nxm 에 맞도록 인덱스 매핑함."
    after: "칸 번호는 좌하단에서 좌상단까지 nxm에 맞도록 인덱스를 매핑한다."
residual_findings: (없음)
grade_reason: "A — S1 0건, 변경률 18.5%, 자체검증 6항 통과. 기술 용어·코드·식 전부 원형 보존, 메모형 단편을 리포트 평서체로 정규화."
-->
