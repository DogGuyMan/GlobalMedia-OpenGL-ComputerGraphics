// apps/_MyApp_/shaders_slang/invert.slang
// postfx 색반전 - 자립 NDC VS + FS. uScene 샘플러만 (값 uniform 없음 -> MaterialBlock 없음).
// 생성물: postprocess/invert.{vs,fs}.

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
    float4 pixel = uScene.Sample(input.uv);
    return float4(1.0 - pixel.rgb, 1.0);
}
