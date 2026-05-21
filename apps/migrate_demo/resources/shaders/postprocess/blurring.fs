#version 410 core

// migrate_demo PostFX FS — 가우시안 근사 블러 (Pascal kernel 7-tap 2D 외적).
// 레퍼런스 OpenGL-With-CMake/resources/shader/postprocess/blurring.fs 회귀.

in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D uScene;
uniform float     uGamma;

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

    // 7-tap 1D 커널 (Pascal 삼각형 6행). 원본 합 = 64.
    const float w[7] = float[](1.0, 6.0, 15.0, 20.0, 15.0, 6.0, 1.0);

    float sum1D = 0.0;
    for (int i = 0; i < 7; ++i)
        sum1D += w[i];

    vec3 color = vec3(0.0);
    for (int y = 0; y < 7; ++y)
    {
        for (int x = 0; x < 7; ++x)
        {
            vec2 off    = vec2(float(x - 3), float(y - 3)) * texel;
            float weight = w[x] * w[y];
            color += texture(uScene, vsTexCoord + off).rgb * weight;
        }
    }

    color /= (sum1D * sum1D);

    // gamma 보정 (uGamma == 1.0 이면 항등).
    color = pow(color, vec3(uGamma));

    fragColor = vec4(color, 1.0);
}
