// apps/_MyApp_/shaders_slang/passthrough.slang
// ScreenQuadStage passthrough/bypass blit - postfx_common 공유 VS + FS.
// 생성물: passthrough.{vs,fs} (resources/shaders 루트 - Constants.h SCREEN_PASSTHROUGH 경로 일치).
import postfx_common;

Sampler2D uScene;   // 직전 패스(또는 비활성 패스 bypass) 출력 색.

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    return float4(uScene.Sample(input.uv).rgb, 1.0);
}
