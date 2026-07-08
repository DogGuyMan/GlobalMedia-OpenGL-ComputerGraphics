// apps/_MyApp_/shaders_slang/sobel.slang
// postfx Sobel 엣지 검출 (휘도 기반) - 자립 NDC VS + FS. uScene 만. 생성물: postprocess/sobel.{vs,fs}.

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

    float3 c[9];
    Sample3x3(uScene, input.uv, texel, c);

    float l[9];
    for (int i = 0; i < 9; ++i)
        l[i] = Luminance(c[i]);

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

    float gx = Dot9(l, sobelX);
    float gy = Dot9(l, sobelY);

    float edge = sqrt(gx * gx + gy * gy);
    edge = clamp(edge, 0.0, 1.0);
    const float threshold = 0.3;
    edge = step(threshold, edge);

    return float4(edge, edge, edge, 1.0);
}
