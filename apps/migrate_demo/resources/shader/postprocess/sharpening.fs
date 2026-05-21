#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션.

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(uScene, 0));

    vec2 offsets[9] = vec2[](
        vec2(-texel.x,  texel.y), vec2(0.0,  texel.y), vec2(texel.x,  texel.y),
        vec2(-texel.x,  0.0),     vec2(0.0,  0.0),     vec2(texel.x,  0.0),
        vec2(-texel.x, -texel.y), vec2(0.0, -texel.y), vec2(texel.x, -texel.y)
    );

    float laplacian[9] = float[](
        -1.0, -1.0, -1.0,
        -1.0,  9.0, -1.0,
        -1.0, -1.0, -1.0
    );

    // 9-이웃 색의 커널 가중합
    vec3 color = vec3(0.0);
    for (int i = 0; i < 9; ++i)
        color += texture(uScene, vUV + offsets[i]).rgb * laplacian[i];

    fragColor = vec4(color, 1.0);   // 컬러 그대로 출력
}
