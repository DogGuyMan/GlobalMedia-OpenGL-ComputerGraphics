// apps/_MyApp_/shaders_slang/simple.slang
// 기존 simple.vs + simple.fs 등가 (라이팅 무관 단색 출력).
// 저작 규칙(D11): 갱신 빈도별 3블록 분할 - FrameBlock(per-frame view/proj)
//                / DrawBlock(per-draw model) / MaterialBlock(per-material baseColor).
// 생성물: simple.vs (vsMain) / simple.fs (fsMain) + simple.refl.json

struct FrameBlock
{
    float4x4 uView;
    float4x4 uProj;
};
ConstantBuffer<FrameBlock> uFrame;

struct DrawBlock
{
    float4x4 uModel;
};
ConstantBuffer<DrawBlock> uDraw;

struct MaterialBlock
{
    float4 baseColor;
};
ConstantBuffer<MaterialBlock> uMaterial;

struct VSIn
{
    float3 aPos : POSITION;   // location 0 - SJH::Vertex 레이아웃 일치
};

[shader("vertex")]
float4 vsMain(VSIn input) : SV_Position
{
    return mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0))));
}

[shader("fragment")]
float4 fsMain() : SV_Target
{
    return uMaterial.baseColor;
}
