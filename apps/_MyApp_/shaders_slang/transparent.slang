// apps/_MyApp_/shaders_slang/transparent.slang
// 기존 transparent.vs + transparent.fs 등가 - 반투명 unlit emissive (벽/PoliceTape).
// 라이팅 무관 자체발광 텍스처를 alpha-blend, alpha<0.1 discard. Pass::RenderQueue::Transparent 가
//   blend(SRC_ALPHA,ONE_MINUS) + depthWrite off + cull off(양면) 자동 도출 (mesh_pass 적용).
// UV = 타일링(uvScale) 후 U 축을 시간(uTime)에 따라 스크롤 (PoliceTape 흐름).
// 3블록 UBO(Frame/Draw/Material) + emissive 샘플러(loose - GL4.1 UBO 불가).
// 생성물: transparent.vs (vsMain) / transparent.fs (fsMain) + transparent.refl.json
// ※ tintColor: 구 FS 는 선언만 하고 미적용이었으나, 머티리얼이 white(1,1,1,1)라 적용해도 무변경 -
//    여기선 *적용* 해 uniform 을 유의미화 (white = identity, 시각 회귀 0).

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
    float4 tintColor;     // 곱 틴트 (머티리얼 기본 white)
    float2 uvScale;       // geometry-space 타일링 배수 (REPEAT 전제)
    float  uTime;         // 누적 시간 (MaterialTime 컴포넌트 매 프레임 갱신)
    float  uScrollSpeed;  // U 방향 스크롤 속도 (tile/sec)
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
    output.uv  = input.aTexCoord * uMaterial.uvScale
               + float2(uMaterial.uTime * uMaterial.uScrollSpeed, 0.0);
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, mul(uDraw.uModel, float4(input.aPos, 1.0))));
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 texel = emissive.Sample(input.uv);
    // ! [Task 2.2 데모] bw 그라디언트 텍스처의 휘도(Rec.601)를 알파로 승격 - black=투명(0) .. white=불투명(1).
    // !   discard(하드컷) / blend(페이드) / +DepthWrite 세 모드 차이를 알파 그라디언트로 뚜렷하게 관찰하기 위함.
    // !   (구: color.a 컷오프 - 알파 채널 없는 그라디언트 텍스처에선 무효라 휘도 기반으로 전환.)
    float alpha = dot(texel.rgb, float3(0.299, 0.587, 0.114)) * uMaterial.tintColor.a;
    if (alpha < 0.1)
        discard;
    return float4(texel.rgb * uMaterial.tintColor.rgb, alpha);
}
