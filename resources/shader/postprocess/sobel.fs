#version 330 core

in vec4 vsColor;
in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D frameTexture;

void main()
{
    // 텍셀 1칸 크기 — 텍스처 해상도에서 동적 계산.
    // textureSize 는 상수식이 아니므로 전역 초기화로 두면 비표준 — 반드시 main 안에서.
    vec2 texel = 1.0 / vec2(textureSize(frameTexture, 0));

    vec2 offsets[9] = vec2[](
        vec2(-texel.x,  texel.y), vec2(0.0,  texel.y), vec2(texel.x,  texel.y),
        vec2(-texel.x,  0.0),     vec2(0.0,  0.0),     vec2(texel.x,  0.0),
        vec2(-texel.x, -texel.y), vec2(0.0, -texel.y), vec2(texel.x, -texel.y)
    );

    float sobelX[9] = float[](
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0
    );

    float sobelY[9] = float[](
        -1.0, -2.0, -1.0,
         0.0,  0.0,  0.0,
         1.0,  2.0,  1.0
    );

    // 이웃 9개를 휘도(luminance)로 변환 — 엣지 검출은 grayscale 기준이 표준.
    float lum[9];
    for (int i = 0; i < 9; ++i)
    {
        vec3 rgb = texture(frameTexture, vsTexCoord + offsets[i]).rgb;
        lum[i] = dot(rgb, vec3(0.299, 0.587, 0.114));
    }

    float gx = 0.0;
    float gy = 0.0;
    for (int i = 0; i < 9; ++i)
    {
        gx += lum[i] * sobelX[i];
        gy += lum[i] * sobelY[i];
    }

    float edge = sqrt(gx * gx + gy * gy);
    edge = clamp(edge, 0.0, 1.0);
    const float threshold = 0.3;
    edge = step(threshold, edge);

    fragColor = vec4(vec3(edge), 1.0);
}
