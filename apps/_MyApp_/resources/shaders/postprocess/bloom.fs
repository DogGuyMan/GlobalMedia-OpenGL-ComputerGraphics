#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션 — 다른 PostFX 셰이더와 동일.

// Bloom 파라미터 — bright-pass 추출 + box blur + additive composite.
// 진짜 emissive bloom (MRT) 가 필요하면 SceneRenderer 의 MRT 인프라 도입 필요.
uniform float uBloomThreshold = 0.7;  // 이 휘도 이상만 bloom (0.0 = 전부, 1.0 = 흰색만)
uniform float uBloomSpread    = 1.5;  // 블러 샘플 간격 (1.0 = 1 텍셀)
uniform float uBloomIntensity = 1.0;  // additive 강도

// 가우시안 가중치 — grayscale_vignetting.fs 의 비네팅 계수 exp(-거리제곱 * k) 와 동일한 형태.
// offset 은 텍셀(정수 격자) 단위, falloff 가 클수록 중앙에 집중(블러 약화).
float GaussianWeight(vec2 offset, float falloff)
{
    return exp(-dot(offset, offset) * falloff);
}

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

    // 9x9 가우시안 블러 — threshold 이상 픽셀만 bright-pass 후 가우시안 가중으로 번지게.
    // (기존 균일 가중 box 블러를 가우시안 가중으로 교체. uIntensityMap 대신 uScene bright-pass 사용)
    const float falloff = 0.125; // sigma = 2 텍셀 (반경 4 = 2 sigma).
    vec3 bloom = vec3(0.0);
    float weightSum = 0.0;
    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            vec2 cell = vec2(float(x), float(y));
            float weight = GaussianWeight(cell, falloff);
            vec2 off = cell * texel * uBloomSpread;
            vec3 samp = texture(uScene, vUV + off).rgb;
            // Rec.709 휘도 — sobel.fs / grayscale_vignetting.fs 와 동일.
            float lum = dot(samp, vec3(0.299, 0.587, 0.114));
            bloom += samp * step(uBloomThreshold, lum) * weight;
            weightSum += weight;
        }
    }
    bloom /= weightSum; // 가우시안 가중 정규화

    // 알베도(선명) + 번진 bright-pass * intensity — additive bloom.
    vec3 base = texture(uScene, vUV).rgb;
    fragColor = vec4(base + bloom * uBloomIntensity, 1.0);
}
