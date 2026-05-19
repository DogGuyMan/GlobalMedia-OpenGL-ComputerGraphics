#version 330 core

in vec4 vsColor;
in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D frameTexture;
void main()
{
    // 텍셀 1칸 크기 — 텍스처 해상도에서 동적 계산.
    vec2 texel = 1.0 / vec2(textureSize(frameTexture, 0));

    // 7-tap 1D 커널 (Pascal 삼각형 6행). 원본 합 = 64.
    const float w[7] = float[](1.0, 6.0, 15.0, 20.0, 15.0, 6.0, 1.0);

    const float powExponent   = 1.0 / 8;  // 1회 pow 의 지수 (학습용 const — uniform 승격 가능)
    const int   powIterations = 1;    // pow 반복 횟수 (0 = 원본 커널)

    float wShaped[7];
    float sum1D = 0.0;
    for (int i = 0; i < 7; ++i)
    {
        float v = w[i];
        for (int k = 0; k < powIterations; ++k)
            v = pow(v, powExponent);
        wShaped[i] = v;
        sum1D += v;
    }

    vec3 color = vec3(0.0);
    for (int y = 0; y < 7; ++y)
    {
        for (int x = 0; x < 7; ++x)
        {
            // 중심 (3,3) 기준 -3 ~ +3 텍셀 오프셋
            vec2 off = vec2(float(x - 3), float(y - 3)) * texel;
            float weight = wShaped[x] * wShaped[y]; // 2D 가중치 = 재성형된 1D 외적
            color += texture(frameTexture, vsTexCoord + off).rgb * weight;
        }
    }

    // 정규화 -> 밝기 보존.
    color /= (sum1D * sum1D);

    fragColor = vec4(color, 1.0);
}
