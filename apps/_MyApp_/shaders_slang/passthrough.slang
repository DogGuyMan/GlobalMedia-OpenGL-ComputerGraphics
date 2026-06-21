// apps/_MyApp_/shaders_slang/passthrough.slang
// ScreenQuadStage passthrough/bypass blit - FBO color attachment 을 그대로 출력.
// 자립 NDC VS + FS. uScene 샘플러만 (값 uniform 없음 -> MaterialBlock 없음 = HasUniformBlocks() false).
// 생성물: passthrough.{vs,fs} (resources/shaders 루트 - Constants.h SCREEN_PASSTHROUGH 경로 일치).
// Phase C (C-2) - 구 hand-written GLSL passthrough.{vs,fs} 대체 (전 셰이더 Slang화 완결).

Sampler2D uScene;   // 직전 패스(또는 비활성 패스 bypass) 출력 색.

struct VSIn
{
    [[vk::location(0)]] float3 aPos : POSITION;   // NDC 좌표 (Mesh::CreateScreenQuad)
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
    output.pos = float4(input.aPos, 1.0);   // model/view/proj 우회 - NDC 직접.
    output.uv  = input.aUV;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    return float4(uScene.Sample(input.uv).rgb, 1.0);
}
