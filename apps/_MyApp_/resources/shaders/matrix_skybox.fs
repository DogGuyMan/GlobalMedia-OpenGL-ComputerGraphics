#version 410 core

in vec3 v_TexCoords; // 버텍스 셰이더에서 넘어온 3D 방향 벡터
out vec4 FragColor;

uniform sampler2D chars;
uniform sampler2D noise_tex;
uniform float u_time; // Godot의 TIME을 대체하는 uniform

const vec2 invAtan = vec2(0.15915494309, 0.31830988618); // 1.0 / (2.0 * PI), 1.0 / PI

// 3D 방향 벡터를 2D 구면(Equirectangular) UV 좌표로 변환
vec2 getSphericalUV(vec3 v) {
        vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
        uv *= invAtan;
        uv += 0.5;
        // OpenGL은 Y축이 아래에서 위로 증가하므로, Godot(위에서 아래)와 맞추기 위해 반전
        uv.y = 1.0 - uv.y;
        return uv;
}

void main() {
        vec3 dir = normalize(v_TexCoords);
        vec2 base_uv = getSphericalUV(dir);

        // 스카이박스 맵은 보통 가로가 세로의 2배(2:1 비율)이므로 글자가 정사각형을 유지하도록 타일링
        // (원하는 빗줄기의 촘촘함에 따라 이 값을 조절하세요)
        vec2 grid_uv = base_uv * vec2(64.0, 32.0);

        // Random character
        vec2 char_uv = fract(grid_uv); // 글자 출력을 위해 uv 루프

        // 전체 스카이박스 UV 기준으로 노이즈 샘플링
        float noise = texture(noise_tex, base_uv).g;
        noise = round(noise * 10.0) / 10.0; // 0.1 단위로 스냅하여 완벽한 오프셋 생성

        char_uv.x = (char_uv.x / 10.0) - 0.005; // 텍스처 내 글자 열 분할 오프셋
        char_uv.x += noise; // 노이즈 값에 따라 무작위 글자 선택
        char_uv.x += round(u_time * 0.5 * 10.0) / 10.0; // 시간에 따른 애니메이션 스냅

        // distortion
        float rain = base_uv.y; // 수직 그레디언트 (내리는 비)
        float distortion = texture(noise_tex, base_uv / vec2(1.0, 32.0)).g; // 가로로 긴 노이즈로 왜곡
        distortion = round(distortion * 10.0) / 10.0;

        rain -= round(u_time * 0.2 * 32.0) / 32.0; // 비가 떨어지는 애니메이션 적용
        rain += distortion; // 그레디언트 왜곡
        rain = fract(rain); // 루프
        rain = round(rain * 16.0) / 16.0; // 비 픽셀화 (선택사항)
        rain = pow(rain, 3.0); // 샤프닝
        rain *= 2.0; // 밝기 증폭

        // 최종 색상 = 문자 텍스처 * 비 그레디언트 * 매트릭스 녹색
        vec3 color = texture(chars, char_uv).rgb * rain * vec3(0.0, 1.0, 0.0);
        FragColor = vec4(color, 1.0);
}
