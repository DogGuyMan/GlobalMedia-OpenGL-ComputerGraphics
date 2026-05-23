#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uColorMap;
uniform sampler2D uIntensityMap;
uniform float bloom_spread;
uniform float bloom_intensity;

void main()
{
        // 발광 마스크 해상도 기준 텍셀 크기 — bloom_spread 로 샘플 간격 조정.
        vec2 texel = 1.0 / vec2(textureSize(uIntensityMap, 0));

        // 9x9 박스 블러 — uIntensityMap 만 번지게 한다 (알베도는 선명하게 유지).
        vec3 blur = vec3(0.0);
        for (int y = -4; y <= 4; ++y)
        {
                for (int x = -4; x <= 4; ++x)
                {
                        vec2 off = vec2(float(x), float(y)) * texel * bloom_spread;
                        blur += texture(uIntensityMap, vUV + off).rgb;
                }
        }
        blur /= 81.0; // 9x9 평균

        // 알베도(선명) + 번진 발광 * intensity — additive bloom.
        vec3 base = texture(uColorMap, vUV).rgb;
        fragColor = vec4(base + blur * bloom_intensity, 1.0);
}
