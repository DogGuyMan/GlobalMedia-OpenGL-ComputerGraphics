#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene; // SP4 컨벤션.
void main()
{
        // 텍셀 1칸 크기 — 텍스처 해상도에서 동적 계산.
        vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

        // 15-tap 1D 커널 (Pascal 삼각형 14행). 원본 합 = 16384.
        const float w[15] = float[](1.0, 14.0, 91.0, 364.0, 1001.0, 2002.0, 3003.0, 3432.0, 3003.0, 2002.0, 1001.0, 364.0, 91.0, 14.0, 1.0);
        const float powExponent = 1.0 / 8; // 1회 pow 의 지수 (학습용 const — uniform 승격 가능)
        const int powIterations = 1; // pow 반복 횟수 (0 = 원본 커널)

        float wShaped[15];
        float sum1D = 0.0;
        for (int i = 0; i < 15; ++i)
        {
                float v = w[i];
                for (int k = 0; k < powIterations; ++k)
                        v = pow(v, powExponent);
                wShaped[i] = v;
                sum1D += v;
        }

        vec3 color = vec3(0.0);
        for (int y = 0; y < 15; ++y)
        {
                for (int x = 0; x < 15; ++x)
                {
                        // 중심 (7,7) 기준 -7 ~ +7 텍셀 오프셋
                        vec2 off = vec2(float(x - 7), float(y - 7)) * texel;
                        float weight = wShaped[x] * wShaped[y]; // 2D 가중치 = 재성형된 1D 외적
                        color += texture(uScene, vUV + off).rgb * weight;
                }
        }

        // 정규화 -> 밝기 보존.
        color /= (sum1D * sum1D);

        fragColor = vec4(color, 1.0);
}
