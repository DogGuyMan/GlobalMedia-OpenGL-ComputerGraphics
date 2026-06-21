// apps/_MyApp_/shaders_slang/simple_texture.slang
// 기존 simple_texture.vs + simple_texture.fs 등가 - texture(uTex, uv) * baseColor (라이팅 무관).
// 저작 규칙(D11): 3블록 UBO 분할 - FrameBlock(view/proj) / DrawBlock(model) / MaterialBlock(baseColor).
// 샘플러 uTex 는 GL 4.1 에서 UBO 불가 -> top-level loose (생성 GLSL 이름 'uTex' 유지가
//   BindSamplers 텍스처 바인딩 키 - mesh_pass 가 sampler 만 loose 바인딩, Slice 0.5).
// 생성물: simple_texture.vs (vsMain) / simple_texture.fs (fsMain) + simple_texture.refl.json
// 참고: 기존 GLSL 은 vsPosition/vsNormal 도 out 했으나 FS 가 uv 만 소비 -> varying 은 uv 1개만 (link 함정 회피).

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

// 결합 샘플러 (GLSL 410 = combined sampler2D). 이름 'uTex' = Properties.Textures["uTex"] 바인딩 키.
Sampler2D uTex;

struct VSIn
{
    // SJH::Vertex 레이아웃: aPos=0 / aNormal=1 / aTexCoord=2. 명시 location 으로 texcoord=2 고정
    // (미사용 aNormal 선언 생략 - dead-code 로 location 재배치되는 함정 방지 위해 vk::location 명시).
    [[vk::location(0)]] float3 aPos      : POSITION;
    [[vk::location(2)]] float2 aTexCoord : TEXCOORD0;
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;   // 유일 inter-stage varying (post-process 가 _slangVary0 정규화)
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    VSOut output;
    float4 worldPos = mul(uDraw.uModel, float4(input.aPos, 1.0));
    output.pos      = mul(uFrame.uProj, mul(uFrame.uView, worldPos));
    output.uv       = input.aTexCoord;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    return uTex.Sample(input.uv) * uMaterial.baseColor;
}
