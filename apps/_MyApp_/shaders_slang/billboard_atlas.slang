// apps/_MyApp_/shaders_slang/billboard_atlas.slang
// 기존 billboard_atlas.vs + billboard_atlas.fs 등가 - atlas sprite 빌보드 + flip/roll + hit/dissolve FX.
// 3블록 UBO(Frame/Draw/Material) + uAtlas/uDissolveTex 샘플러(loose). SpriteRenderer(=MeshRenderer)가 사용.
// per-frame: uUvRect(애니 프레임)/uFlipX(방향)/uRoll/uTime(hit blink)/dissolve - Slice 0 매-draw 업로드.
// bool(uEnableHit/uEnableDissolve)은 std140 4바이트 - 머티리얼이 SetInt(0/1)로 설정 -> 일반 업로드(Ints).
// 빌보드 카메라축 = transpose+mul (원소 인덱싱 컨벤션 회피, healthbar/transparent 와 동일 검증 경로).
// 생성물: billboard_atlas.vs/.fs + billboard_atlas.refl.json. Slang: mix->lerp, fract 없음, step/sin 동일.

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
    float4 uUvRect;                   // (uMin,vMin,uSize,vSize) atlas sub-rect (per-frame 애니)
    float4 uTint;                     // 곱 틴트
    float3 uDissolveOutlineColor;     // 디졸브 외곽선 에미시브 색
    float  uDissolveThreshold;        // 디졸브 임계 (0..1)
    float  uDissolveOutlineThickness; // 외곽선 두께
    float  uFlipX;                    // +1 / -1 (좌우반전)
    float  uRoll;                     // roll 회전 (radians)
    float  uTime;                     // hit blink 시간
    bool   uEnableHit;                // 피격 깜빡임
    bool   uEnableDissolve;           // 사망 디졸브
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uAtlas;       // unit 0 - 스프라이트 아틀라스
Sampler2D uDissolveTex; // unit 1 (미지정 시 unit0=atlas crude erode)

struct VSIn
{
    [[vk::location(0)]] float3 aPos      : POSITION;   // XY quad (-0.5..0.5)
    [[vk::location(2)]] float2 aTexCoord : TEXCOORD0;
};

struct VSOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    // 카메라 right/up (world) - transpose(view)·축 (회전 전용, w=0 방향).
    float3 cameraRight = mul(transpose(uFrame.uView), float4(1.0, 0.0, 0.0, 0.0)).xyz;
    float3 cameraUp    = mul(transpose(uFrame.uView), float4(0.0, 1.0, 0.0, 0.0)).xyz;

    // uModel 에서 center + size (mul 기반 - 원소 인덱싱 회피).
    float3 center = mul(uDraw.uModel, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    float  sx     = length(mul(uDraw.uModel, float4(1.0, 0.0, 0.0, 0.0)).xyz);
    float  sy     = length(mul(uDraw.uModel, float4(0.0, 1.0, 0.0, 0.0)).xyz);

    // flip 선반영 후 roll(카메라 정면축) 2D 회전.
    float2 p  = float2(input.aPos.x * uMaterial.uFlipX, input.aPos.y);
    float  cr = cos(uMaterial.uRoll);
    float  sr = sin(uMaterial.uRoll);
    float2 rp = float2(p.x * cr - p.y * sr, p.x * sr + p.y * cr);

    float3 worldPos = center + cameraRight * rp.x * sx + cameraUp * rp.y * sy;

    VSOut output;
    output.uv  = uMaterial.uUvRect.xy + input.aTexCoord * uMaterial.uUvRect.zw;
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, float4(worldPos, 1.0)));
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float4 c = uAtlas.Sample(input.uv);
    if (c.a < 0.01)
        discard;   // alpha-test (sorting 회피)

    // 1. 피격 효과 (빨강/하양 극단 깜빡임)
    if (uMaterial.uEnableHit)
    {
        float  blink    = step(0.0, sin(uMaterial.uTime * 30.0));
        float3 hitColor = lerp(float3(1.0, 1.0, 1.0), float3(1.0, 0.0, 0.0), blink);
        c.rgb = hitColor;
    }

    float4 finalColor = c * uMaterial.uTint;

    // 2. 디졸브 효과 (사망 시 사라짐)
    if (uMaterial.uEnableDissolve)
    {
        float dissolveValue = uDissolveTex.Sample(input.uv).r;
        if (dissolveValue < uMaterial.uDissolveThreshold)
            discard;
        float edge = step(dissolveValue, uMaterial.uDissolveThreshold + uMaterial.uDissolveOutlineThickness);
        finalColor.rgb = lerp(finalColor.rgb, uMaterial.uDissolveOutlineColor, edge);
    }

    return finalColor;
}
