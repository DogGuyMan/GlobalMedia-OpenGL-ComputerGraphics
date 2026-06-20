// apps/_MyApp_/shaders_slang/phong.slang
// PCB 스테이지 모델용 albedo Phong 셰이더 (Phase 2.5). phong_albedo.fs 의 Slang/UBO 등가.
//
// 라이팅 로직은 phong_lighting 모듈에 단일화 - 이 파일은 *진입점 + per-frame/draw/material 블록* 만.
//   import 로 LightBlock UBO + ComputePhong 을 가져와 한 줄 호출 (셰이더별 라이팅 복제 제거).
//
// FBX 에 임베디드 텍스처가 없어 albedo 색(vec3) 기반 (텍스처/sampler 없음).
// shininess 는 상수 32.0 - model.cpp 가 material.shininess 를 항상 32.0 으로 고정하므로 충실도 손실 0.
// normal 행렬은 PoC 단순화로 (float3x3)uModel (균등 스케일 가정).
// 생성물: phong.vs (vsMain) / phong.fs (fsMain) + phong.refl.json.

import phong_lighting;   // LightBlock UBO + ComputePhong (라이팅 SSOT)

static const float SHININESS = 32.0;   // model.cpp material.shininess 고정값과 동일

// === per-frame / per-draw / per-material (mesh_pass 갱신 - simple.slang 과 동형) ===
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
    float4 baseColor;   // .rgb = albedo (FBX aiColor_Diffuse). simple 과 동일 레이아웃이라 mesh_pass 무변경.
};
ConstantBuffer<MaterialBlock> uMaterial;

// === 정점 IO (aTexCoord 는 albedo 셰이더라 미사용 - location 2 는 비워둠) ===
struct VSIn
{
    float3 aPos    : POSITION;   // loc 0 - SJH::Vertex 레이아웃
    float3 aNormal : NORMAL;     // loc 1
};

struct VSOut
{
    float4 pos     : SV_Position;
    float3 wPos    : POSITION0;   // world position
    float3 wNormal : NORMAL0;     // world normal
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    VSOut o;
    float4 world = mul(uDraw.uModel, float4(input.aPos, 1.0));
    o.wPos    = world.xyz;
    o.wNormal = mul((float3x3)uDraw.uModel, input.aNormal);  // PoC 단순화 (균등 스케일 가정)
    o.pos     = mul(uFrame.uProj, mul(uFrame.uView, world));
    return o;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float3 albedo = uMaterial.baseColor.rgb;     // .rgb = FBX albedo
    float3 N      = normalize(input.wNormal);

    // 전체 Phong 조명 = 모듈 1줄 호출 (LightBlock UBO + dir/point/spot 루프 + V 도출 내부 캡슐화).
    float3 result = ComputePhong(albedo, SHININESS, N, input.wPos);
    return float4(result, 1.0);
}
