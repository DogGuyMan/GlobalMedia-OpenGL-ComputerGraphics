// apps/_MyApp_/shaders_slang/blurring.slang
// postfx 15x15 가우시안 블러 - 자립 NDC VS + FS. uScene 샘플러만 (값 uniform 없음).
// 생성물: postprocess/blurring.{vs,fs}. textureSize -> GetDimensions.

import postfx_common;
import image_processing;

Sampler2D uScene;

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float2 texel = TexelSize(uScene);

    const float falloff = 0.08;   // sigma = 2.5 텍셀
    float3 color     = float3(0.0, 0.0, 0.0);
    float  weightSum = 0.0;
    for (int y = -7; y <= 7; ++y)
    {
        for (int x = -7; x <= 7; ++x)
        {
            float2 cell   = float2(float(x), float(y));
            float  weight = GaussianWeight(cell, falloff);
            color     += uScene.Sample(input.uv + cell * texel).rgb * weight;
            weightSum += weight;
        }
    }
    color /= weightSum;
    return float4(color, 1.0);
}
