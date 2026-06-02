#version 410 core

in vec2 vUv;

uniform float uFill;           // 체력 비율 0..1 (HealthBarDriver 가 매 프레임 갱신)
uniform vec4  uColor;          // 채워진 조각 색
uniform vec4  uBgColor;        // 빈 조각(트랙) 색
uniform float uSegmentCount;   // 조각 수
uniform float uSegmentSpacing; // 조각 내 half-gap 비율 (fract 단위)

out vec4 fragColor;

void main()
{
    float u = clamp(vUv.x, 0.0, 1.0);
    float N = max(uSegmentCount, 1.0);

    // 현재 조각 내부 좌표 + 조각 경계까지 거리
    float f    = fract(u * N);
    float edge = min(f, 1.0 - f);

    // fwidth 안티에일리어싱 (레퍼런스 사상)
    float aaSeg = fwidth(u * N);
    float aaU   = fwidth(u);

    // 조각 몸체 = 1, 조각 사이 간격(gap) = 0 (gap 은 투명)
    float body = smoothstep(uSegmentSpacing, uSegmentSpacing + aaSeg, edge);

    // 좌→우 채움: u < uFill 채워짐 (우측부터 비워짐), 경계 AA
    float fill = 1.0 - smoothstep(uFill - aaU, uFill + aaU, u);

    // 채워진 조각=uColor, 빈 조각=어두운 uBgColor(트랙). gap 은 alpha*body 로 투명.
    vec3  rgb = mix(uBgColor.rgb, uColor.rgb, fill);
    float a   = mix(uBgColor.a,  uColor.a,  fill) * body;

    fragColor = vec4(rgb, a);
}
