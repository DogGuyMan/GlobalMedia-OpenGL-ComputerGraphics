// apps/_MyApp_/shaders_slang/simple.slang
// 기존 simple.vs + simple.fs 등가 (라이팅 무관 단색 출력).
// 저작 규칙(spec 3.1): 전역 상수/행렬은 ConstantBuffer 로 묶어 UBO 화.
// 생성물: simple.vs (vsMain) / simple.fs (fsMain) + simple.refl.json

struct CameraBlock
{
    float4x4 uModel;
    float4x4 uView;
    float4x4 uProj;
    float4   baseColor;
};
ConstantBuffer<CameraBlock> uCamera;

struct VSIn
{
    float3 aPos : POSITION;   // location 0 — SJH::Vertex 레이아웃 일치
};

[shader("vertex")]
float4 vsMain(VSIn input) : SV_Position
{
    return mul(uCamera.uProj, mul(uCamera.uView, mul(uCamera.uModel, float4(input.aPos, 1.0))));
}

[shader("fragment")]
float4 fsMain() : SV_Target
{
    return uCamera.baseColor;
}
