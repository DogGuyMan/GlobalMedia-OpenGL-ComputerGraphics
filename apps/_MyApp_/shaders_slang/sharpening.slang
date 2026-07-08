// apps/_MyApp_/shaders_slang/sharpening.slang
// postfx 라플라시안 샤프닝 (3x3 커널) - 자립 NDC VS + FS. uScene 만. 생성물: postprocess/sharpening.{vs,fs}.

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

    float laplacian[9] = {
        -1.0, -1.0, -1.0,
        -1.0,  9.0, -1.0,
        -1.0, -1.0, -1.0
    };

    float3 color = Dot9Color(c, laplacian);
    return float4(color, 1.0);
}
