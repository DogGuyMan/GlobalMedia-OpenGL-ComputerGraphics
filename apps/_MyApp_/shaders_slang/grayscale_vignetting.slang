// apps/_MyApp_/shaders_slang/grayscale_vignetting.slang
// postfx grayscale + vignetting - 자립 NDC VS + FS. 생성물: postprocess/grayscale_vignetting.{vs,fs}.
// ★ D-DPP-1(b) 런타임키 검증: uGrayscaleAmount(HpGrayscalePostFX), uVignetteAmount(PostFXTweenPlayable)는
//   런타임 string 키로 Properties.Floats 에 기록 -> Slice 0 일반 이름매칭 업로드가 자동 처리(특수코드 0).
// uVignetteColor(vec3)는 startup/ImGui 가 Properties.Vec3s 로 set -> 동일 일반 업로드.

import postfx_common;
import image_processing;

struct MaterialBlock
{
    float  uGrayscaleAmount;  // 1=원본, 0=무채색 (HP ratio 런타임 구동)
    float  uVignetteAmount;   // 0=없음, 1=강함 (hit flash 트윈 런타임 구동)
    float3 uVignetteColor;    // 테두리 색
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    return ComputeScreenQuadVSOut(input);
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float3 finalColor = uScene.Sample(input.uv).rgb;

    // 1. GrayScale - Rec.709 휘도 후 mix.
    float  luminance = Luminance(finalColor);
    float3 grayColor = float3(luminance, luminance, luminance);
    finalColor = lerp(grayColor, finalColor, uMaterial.uGrayscaleAmount);

    // 2. Vignetting - 중앙 거리 기반 가우시안.
    float dist           = distance(input.uv, float2(0.5, 0.5));
    float intensity      = uMaterial.uVignetteAmount * 20.0;
    float vignetteFactor = exp(-dist * dist * intensity);
    finalColor = lerp(uMaterial.uVignetteColor, finalColor, vignetteFactor);

    return float4(finalColor, 1.0);
}
