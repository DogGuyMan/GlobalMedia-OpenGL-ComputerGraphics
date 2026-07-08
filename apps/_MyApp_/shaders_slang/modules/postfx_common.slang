// apps/_MyApp_/shaders_slang/postfx_common.slang
// 풀스크린 NDC passthrough VS 공통 - entry point(vsMain)는 각 파일에 로컬 유지(D-1),
// 몸통 로직만 함수로 공유.
module postfx_common;

public struct VSIn
{
    [[vk::location(0)]] float3 aPos : POSITION;
    [[vk::location(2)]] float2 aUV  : TEXCOORD0;
};

public struct VSOut
{
    public float4 pos : SV_Position;
    public float2 uv  : TEXCOORD0;
};

public VSOut ComputeScreenQuadVSOut(VSIn input)
{
    VSOut output;
    output.pos = float4(input.aPos, 1.0);
    output.uv  = input.aUV;
    return output;
}
