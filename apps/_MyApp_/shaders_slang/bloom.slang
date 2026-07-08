// apps/_MyApp_/shaders_slang/bloom.slang
// postfx bloom (bright-pass + 9x9 가우시안 + additive) - 자립 NDC VS + FS.
// MaterialBlock{uBloomThreshold/uBloomSpread/uBloomIntensity} + uScene. 생성물: postprocess/bloom.{vs,fs}.

import postfx_common;
import image_processing;

struct MaterialBlock
{
    float uBloomThreshold;  // 이 휘도 이상만 bloom
    float uBloomSpread;     // 블러 샘플 간격
    float uBloomIntensity;  // additive 강도
};
ConstantBuffer<MaterialBlock> uMaterial;

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

    const float falloff = 0.125;   // sigma = 2 텍셀
    float3 bloom     = float3(0.0, 0.0, 0.0);
    float  weightSum = 0.0;
    for (int y = -4; y <= 4; ++y)
    {
        for (int x = -4; x <= 4; ++x)
        {
            float2 cell   = float2(float(x), float(y));
            float  weight = GaussianWeight(cell, falloff);
            float2 off    = cell * texel * uMaterial.uBloomSpread;
            float3 samp   = uScene.Sample(input.uv + off).rgb;
            float  lum    = Luminance(samp);
            bloom     += samp * step(uMaterial.uBloomThreshold, lum) * weight;
            weightSum += weight;
        }
    }
    bloom /= weightSum;

    float3 base = uScene.Sample(input.uv).rgb;
    return float4(base + bloom * uMaterial.uBloomIntensity, 1.0);
}
