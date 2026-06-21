// apps/_MyApp_/shaders_slang/sharpening.slang
// postfx 라플라시안 샤프닝 (3x3 커널) - 자립 NDC VS + FS. uScene 만. 생성물: postprocess/sharpening.{vs,fs}.

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
    float laplacian[9] = {
        -1.0, -1.0, -1.0,
        -1.0,  9.0, -1.0,
        -1.0, -1.0, -1.0
    };

    float3 color = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 9; ++i)
        color += uScene.Sample(input.uv + offsets[i]).rgb * laplacian[i];

    return float4(color, 1.0);
}
