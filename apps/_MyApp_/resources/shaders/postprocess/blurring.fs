#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene; // SP4 컨벤션.

// 가우시안 가중치 — grayscale_vignetting.fs 의 비네팅 계수 exp(-거리제곱 * k) 와 동일한 형태.
// offset 은 텍셀(정수 격자) 단위, falloff 가 클수록 중앙에 집중(블러 약화).
float GaussianWeight(vec2 offset, float falloff)
{
        return exp(-dot(offset, offset) * falloff);
}

void main()
{
        // 텍셀 1칸 크기 — 텍스처 해상도에서 동적 계산.
        vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

        // 15x15 가우시안 블러 — 기존 Pascal(이항계수) 커널을 exp(-거리제곱) 가우시안으로 교체.
        const float falloff = 0.08; // sigma = 2.5 텍셀 (반경 7 = 2.8 sigma).
        vec3 color = vec3(0.0);
        float weightSum = 0.0;
        for (int y = -7; y <= 7; ++y)
        {
                for (int x = -7; x <= 7; ++x)
                {
                        // 중심 기준 -7 ~ +7 텍셀 오프셋
                        vec2 cell = vec2(float(x), float(y));
                        float weight = GaussianWeight(cell, falloff);
                        color += texture(uScene, vUV + cell * texel).rgb * weight;
                        weightSum += weight;
                }
        }

        // 정규화 -> 밝기 보존.
        color /= weightSum;

        fragColor = vec4(color, 1.0);
}
