#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션 — 다른 PostFX 셰이더와 동일.

// Bloom 파라미터 — bright-pass 추출 + box blur + additive composite.
// 진짜 emissive bloom (MRT) 가 필요하면 SceneRenderer 의 MRT 인프라 도입 필요.
uniform float uBloomThreshold = 0.7;  // 이 휘도 이상만 bloom (0.0 = 전부, 1.0 = 흰색만)
uniform float uBloomSpread    = 1.5;  // 블러 샘플 간격 (1.0 = 1 텍셀)
uniform float uBloomIntensity = 1.0;  // additive 강도

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

    // 9x9 박스 블러 — threshold 이상 픽셀만 추출 후 번지게.
    // 원래 bloom.fs 의 uIntensityMap 자리에 uScene 의 bright-pass 사용.
    vec3 bloom = vec3(0.0);
    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            vec2 off = vec2(float(x), float(y)) * texel * uBloomSpread;
            vec3 samp = texture(uScene, vUV + off).rgb;
            // Rec.709 휘도 — sobel.fs / grayscale_vignetting.fs 와 동일.
            float lum = dot(samp, vec3(0.299, 0.587, 0.114));
            bloom += samp * step(uBloomThreshold, lum);
        }
    }
    bloom /= 81.0; // 9x9 평균

    // 알베도(선명) + 번진 bright-pass * intensity — additive bloom.
    vec3 base = texture(uScene, vUV).rgb;
    fragColor = vec4(base + bloom * uBloomIntensity, 1.0);
}
