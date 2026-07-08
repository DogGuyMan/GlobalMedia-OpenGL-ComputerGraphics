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

// std140: tintColor @0(16) / uvScale @16(8) / uTime @24(4) / uScrollSpeed @28(4) - 총 32.
struct MaterialBlock
{
	float4 tintColor; // 곱 틴트 (머티리얼 기본 white)
	float2 uvScale;
	float2 uvOffset;
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
	output.uv = (input.aTexCoord * uMaterial.uvScale) + uMaterial.uvOffset;
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0))));
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 texel = emissive.Sample(input.uv);
    if (texel.a < 0.1)
        discard;
    return float4(texel.rgba);
}
