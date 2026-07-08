// apps/_MyApp_/shaders_slang/sobel.slang
// postfx Sobel 엣지 검출 (휘도 기반) - 자립 NDC VS + FS. uScene 만. 생성물: postprocess/sobel.{vs,fs}.

import postfx_common;

Sampler2D uScene;

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    uint texW, texH;
    uScene.GetDimensions(texW, texH);
    float2 texel = 1.0 / float2(texW, texH);

    float2 offsets[9] = {
        float2(-texel.x,  texel.y), float2(0.0,  texel.y), float2(texel.x,  texel.y),
        float2(-texel.x,  0.0),     float2(0.0,  0.0),     float2(texel.x,  0.0),
        float2(-texel.x, -texel.y), float2(0.0, -texel.y), float2(texel.x, -texel.y)
    };
    float sobelX[9] = {
        -1.0, 0.0, 1.0,
        -2.0, 0.0, 2.0,
        -1.0, 0.0, 1.0
    };
    float sobelY[9] = {
        -1.0, -2.0, -1.0,
         0.0,  0.0,  0.0,
         1.0,  2.0,  1.0
    };

    float lum[9];
    for (int i = 0; i < 9; ++i)
    {
        float3 rgb = uScene.Sample(input.uv + offsets[i]).rgb;
        lum[i] = dot(rgb, float3(0.299, 0.587, 0.114));
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

    return float4(edge, edge, edge, 1.0);
}
