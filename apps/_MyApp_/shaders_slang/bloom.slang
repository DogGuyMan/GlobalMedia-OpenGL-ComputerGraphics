// apps/_MyApp_/shaders_slang/bloom.slang
// postfx bloom (bright-pass + 9x9 가우시안 + additive) - 자립 NDC VS + FS.
// MaterialBlock{uBloomThreshold/uBloomSpread/uBloomIntensity} + uScene. 생성물: postprocess/bloom.{vs,fs}.

struct MaterialBlock
{
    float uBloomThreshold;  // 이 휘도 이상만 bloom
    float uBloomSpread;     // 블러 샘플 간격
    float uBloomIntensity;  // additive 강도
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;

struct VSIn
{
    [[vk::location(0)]] float3 aPos : POSITION;
    [[vk::location(2)]] float2 aUV  : TEXCOORD0;
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    VSOut output;
    output.pos = float4(input.aPos, 1.0);
    output.uv  = input.aUV;
    return output;
}

float GaussianWeight(float2 offset, float falloff)
{
    return exp(-dot(offset, offset) * falloff);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    uint texW, texH;
    uScene.GetDimensions(texW, texH);
    float2 texel = 1.0 / float2(texW, texH);

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
            float  lum    = dot(samp, float3(0.299, 0.587, 0.114));
            bloom     += samp * step(uMaterial.uBloomThreshold, lum) * weight;
            weightSum += weight;
        }
    }
    bloom /= weightSum;

    float3 base = uScene.Sample(input.uv).rgb;
    return float4(base + bloom * uMaterial.uBloomIntensity, 1.0);
}
