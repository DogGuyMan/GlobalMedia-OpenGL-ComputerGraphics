// apps/_MyApp_/shaders_slang/gamma.slang
// postfx gamma 보정 - 자체 NDC passthrough VS + FS (자립형, 공유 postprocess.vs 미사용).
// Phase 3 Slice 2 (옵션 B) - effect 별 자립 VS+FS 로 점진 전환. 생성물: postprocess/gamma.{vs,fs}.
// uScene 샘플러(loose) + MaterialBlock{gamma}. mesh_pass ScreenQuad 경로가 UBO 멤버 업로드.

struct MaterialBlock
{
    float gamma;
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;   // 직전 패스 출력 (이름 = Properties.Textures["uScene"], mesh_pass 가 매 패스 주입)

struct VSIn
{
    // CreateScreenQuad NDC 좌표 직접 사용. aPos(loc0) / aUV(loc2).
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
    output.pos = float4(input.aPos, 1.0);   // NDC 직접 (model/view/proj 우회)
    output.uv  = input.aUV;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 pixel = uScene.Sample(input.uv);
    return float4(pow(pixel.rgb, float3(uMaterial.gamma)), 1.0);
}
