// apps/_MyApp_/shaders_slang/gamma.slang
// postfx gamma 보정 - 자체 NDC passthrough VS + FS (자립형, 공유 postprocess.vs 미사용).
// Phase 3 Slice 2 (옵션 B) - effect 별 자립 VS+FS 로 점진 전환. 생성물: postprocess/gamma.{vs,fs}.
// uScene 샘플러(loose) + MaterialBlock{gamma}. mesh_pass ScreenQuad 경로가 UBO 멤버 업로드.

import postfx_common;

struct MaterialBlock
{
    float gamma;
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;   // 직전 패스 출력 (이름 = Properties.Textures["uScene"], mesh_pass 가 매 패스 주입)

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 pixel = uScene.Sample(input.uv);
    return float4(pow(pixel.rgb, float3(uMaterial.gamma)), 1.0);
}
