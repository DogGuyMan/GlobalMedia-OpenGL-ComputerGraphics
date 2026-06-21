// apps/_MyApp_/shaders_slang/fog.slang
// postfx 거리 안개 (depth 재구성) - 자립 NDC VS + FS. 생성물: postprocess/fog.{vs,fs}.
// uScene/uDepth 샘플러(loose) + MaterialBlock{uInverseProjection(mat4)/uFogColor(vec3)/density/start/end/mode}.
// NDC->view 복원: uInverseProjection * ndc (perspective divide). uFogMode 0=Linear/1=Exp/2=Exp2.
// uInverseProjection 은 per-frame(WorldCamera proj 역행렬) -> Slice 0 매-draw 업로드. mix->lerp.

struct MaterialBlock
{
    float4x4 uInverseProjection;  // inverse(WorldCamera projection) - NDC->view 복원
    float3   uFogColor;
    float    uFogDensity;
    float    uFogStart;           // Linear 모드 시작 거리 (view 거리 단위)
    float    uFogEnd;             // Linear 모드 fully-fogged 거리
    int      uFogMode;            // 0=Linear, 1=Exp, 2=Exp2
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;   // 직전 패스 출력 (색)
Sampler2D uDepth;   // sceneFB depth (.r = 정규화 depth)

struct VSIn
{
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
    output.pos = float4(input.aPos, 1.0);
    output.uv  = input.aUV;
    return output;
}

// glsl-fog 레퍼런스 (dist = view-space 유클리디안 거리).
float fogFactorLinear(float dist, float start, float end)
{
    return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}
float fogFactorExp(float dist, float density)
{
    return 1.0 - clamp(exp(-density * dist), 0.0, 1.0);
}
float fogFactorExp2(float dist, float density)
{
    const float LOG2 = -1.442695;
    float d = density * dist;
    return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float3 sceneColor = uScene.Sample(input.uv).rgb;
    float  rawDepth   = uDepth.Sample(input.uv).r;

    // D3 - skybox/배경(far plane, depth≈1.0)은 fog 제외 (하늘 또렷).
    if (rawDepth >= 0.9999)
        return float4(sceneColor, 1.0);

    // NDC -> view-space 복원 (perspective divide).
    float4 ndc     = float4(input.uv * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
    float4 viewPos = mul(uMaterial.uInverseProjection, ndc);
    viewPos /= viewPos.w;
    float dist = length(viewPos.xyz);

    float fogAmount;
    if (uMaterial.uFogMode == 0)
        fogAmount = fogFactorLinear(dist, uMaterial.uFogStart, uMaterial.uFogEnd);
    else if (uMaterial.uFogMode == 1)
        fogAmount = fogFactorExp(dist, uMaterial.uFogDensity);
    else
        fogAmount = fogFactorExp2(dist, uMaterial.uFogDensity);

    return float4(lerp(sceneColor, uMaterial.uFogColor, fogAmount), 1.0);
}
