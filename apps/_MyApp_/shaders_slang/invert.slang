// apps/_MyApp_/shaders_slang/invert.slang
// postfx 색반전 - 자립 NDC VS + FS. uScene 샘플러만 (값 uniform 없음 -> MaterialBlock 없음).
// 생성물: postprocess/invert.{vs,fs}.

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
    float4 pixel = uScene.Sample(input.uv);
    return float4(1.0 - pixel.rgb, 1.0);
}
