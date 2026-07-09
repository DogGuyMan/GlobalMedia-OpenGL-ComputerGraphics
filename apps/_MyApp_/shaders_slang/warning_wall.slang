// apps/_MyApp_/shaders_slang/warning_wall.slang
// transparent 기반 - 시간에 따라 U축 스크롤하는 경고 벽(PoliceTape) 반투명 unlit emissive.
// uTime 은 main.cpp 렌더루프가 매 프레임 주입(skybox u_time 패턴, capture 결정적).
// Pass::RenderQueue::Transparent (blend on, depthWrite off, cull off 양면).
// std140: tintColor @0(16) / uvScale @16(8) / uTime @24(4) / uScrollSpeed @28(4) - 총 32.
// 생성물: warning_wall.vs (vsMain) / warning_wall.fs (fsMain) + warning_wall.refl.json.

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
    float4 tintColor;    // 곱 틴트 (머티리얼 기본 white)
    float2 uvScale;      // geometry-space 타일링 배수 (텍스처 REPEAT 전제)
    float  uTime;        // 누적 시간 (main.cpp 매 프레임 주입 - skybox u_time 패턴)
    float  uScrollSpeed; // U 방향 스크롤 속도 (tile/sec)
};
ConstantBuffer<MaterialBlock> uMaterial;

// 자체발광 텍스처 (unit 0). 이름 'emissive' = Properties.Textures["emissive"] 바인딩 키.
Sampler2D emissive;

struct VSIn
{
    // SJH::Vertex: aPos=0 / aNormal=1(미사용) / aTexCoord=2. 명시 location 으로 texcoord=2 고정.
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
    // 타일링(uvScale) 후 U 축을 시간(uTime*uScrollSpeed)으로 스크롤.
    output.uv  = input.aTexCoord * uMaterial.uvScale
               + float2(uMaterial.uTime * uMaterial.uScrollSpeed, 0.0);
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0))));
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 texel = emissive.Sample(input.uv);
    // 투명 픽셀 컷오프 (PoliceTape 는 RGBA - alpha 채널이 띠 형상).
    if (texel.a < 0.1)
        discard;
    return float4(texel.rgb * uMaterial.tintColor.rgb, texel.a);
}
