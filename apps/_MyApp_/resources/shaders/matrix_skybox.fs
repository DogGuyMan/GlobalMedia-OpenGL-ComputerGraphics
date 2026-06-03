#version 410 core

in vec3 v_TexCoords; // 버텍스 셰이더에서 넘어온 3D 방향 벡터
out vec4 FragColor;

uniform sampler2D chars;
uniform sampler2D noise_tex;
uniform float u_time; // Godot의 TIME을 대체하는 uniform

const vec2 invAtan = vec2(0.15915494309, 0.31830988618);

// 격자 밀도 (클수록 글자가 작고 촘촘 — 취향껏 조절)
//    가로:세로 = 2:1 유지 권장 (equirectangular 가 2:1 이므로 글자 비율 보존).
const float COLS = 256.0;
const float ROWS = 128.0;

// characters.png 아틀라스 실측 레이아웃─
//    글자 10개, 각 글자칸 64x128, 글자간/양끝 여백 4px -> 총 폭 4+(64+4)*10 = 684.
//    글자 d 의 칸 좌측(px) = MARGIN + d*(CHAR_W + GAP).
const float CHAR_W = 64.0;
const float GAP = 4.0; // 글자간 간격 = 양끝 여백
const float ATLAS_W = 684.0; // = GAP + (CHAR_W + GAP) * 10
const float CHAR_CNT = 10.0;

// 노이즈 샘플 주파수 — 클수록 노이즈가 작고 촘촘 (간격 1/NOISE_SCALE).
// noise_tex 가 REPEAT wrap 이어야 좌표 >1 이 클램프 없이 타일링된다 (main.cpp 에서 설정).
const float NOISE_SCALE = 2.0;

// 노이즈 UV 오프셋 — 노이즈 패턴을 통째로 이동 (글자를 자르는 rain 띠 위치 조정용).
//    .x = 가로 이동, .y = 세로 이동. distortion(아래 rain)에 가장 큰 영향.
const vec2 NOISE_OFFSET = vec2(0.5, 0.0);

// 3D 방향 벡터를 2D 구면(Equirectangular) UV 좌표로 변환
vec2 getSphericalUV(vec3 v) {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        // OpenGL은 Y축이 아래에서 위로 증가하므로, Godot(위에서 아래)와 맞추기 위해 반전
        return uv;
}

void main() {
        vec3 dir = normalize(v_TexCoords);
        vec2 base_uv = getSphericalUV(dir);

        // 글자 격자 셀 분해
        vec2 grid_uv = base_uv * vec2(COLS, ROWS);
        vec2 cellId = floor(grid_uv); // 정수 셀 좌표
        vec2 inCell = fract(grid_uv); // 셀 내부 위치 [0,1)

        // 디지트 선택 — *셀당 한 번* 샘플 (셀 내부에서 일정해야 한 글자만 보임).
        //    예전엔 per-fragment 라 셀 안에서 글자가 갈라져 어긋나 보였다.
        float g = texture(noise_tex, cellId / vec2(COLS, ROWS) * NOISE_SCALE + NOISE_OFFSET).g;
        float d = mod(floor(g * CHAR_CNT) + floor(u_time * 5.0), CHAR_CNT); // 0..9 + 시간 스크롤

        // 텍스처 아틀라스 정확 매핑 — d 번째 64px 글자칸만 샘플 (간격/여백 제외)
        float cellLeftPx = GAP + d * (CHAR_W + GAP);
        vec2 char_uv = vec2(
                        (cellLeftPx + inCell.x * CHAR_W) / ATLAS_W, // 가로: 정확히 64px 글자칸
                        inCell.y); // 세로: 아틀라스 전체 높이(128)

        // 내리는 비(rain) — 세로 그레디언트 + 노이즈 왜곡
        float distortion = texture(noise_tex, vec2(cellId.x / COLS, 0.0) * NOISE_SCALE + NOISE_OFFSET).g; // 열(column)별 낙하 위상 — 글자 셀 가로(width)에 정렬
        distortion = round(distortion * 10.0) / 10.0;

        float rain = cellId.y / ROWS; // 세로 그레디언트 — 글자 셀 행(UV)에 정렬 (셀 단위 양자화, 중간 안 잘림)
        rain += round(u_time * 0.2 * ROWS) / ROWS; // 비/노이즈 스크롤 방향 (부호로 반전: -= ↔ +=)
        rain += distortion; // 그레디언트 왜곡
        rain = fract(rain); // 루프
        rain = round(rain * 16.0) / 16.0; // 비 픽셀화
        rain = pow(rain, 3.0); // 샤프닝
        rain *= 2.0; // 밝기 증폭

        // 최종 색상 = 문자 텍스처 * 비 그레디언트 * 매트릭스 녹색
        vec3 color = texture(chars, char_uv).rgb * rain * vec3(0.0, 1.0, 0.0);
        FragColor = vec4(color, 1.0);
}
