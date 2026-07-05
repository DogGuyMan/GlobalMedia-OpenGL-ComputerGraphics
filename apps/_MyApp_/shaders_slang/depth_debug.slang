// apps/_MyApp_/shaders_slang/depth_debug.slang
// Phase 1 학습용 - 씬 깊이 버퍼(sceneFB depth)를 흑백으로 출력.
// 목적: ZWrite 가 쓰는 것이 '색'이 아니라 이 '깊이 이미지'임을 눈으로 확인 +
//       깊이가 near 쪽에 몰리는 '비선형'을 raw vs linearized 비교로 체감.
// 생성물: postprocess/depth_debug.{vs,fs}. fog.slang 을 극단 단순화한 형태.
// uLinearize 0 = raw(비선형, 온통 흰색) / 1 = 선형화(near~far 고른 회색).

struct MaterialBlock
{
    float uNear;       // 카메라 near 평면 (view 거리). WorldCamera 값과 맞추면 정확.
    float uFar;        // 카메라 far 평면.
    float uLinearize;  // 0 = raw NDC depth 그대로 / 1 = view-space 선형화
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D uScene;   // 직전 패스 출력(색) - 여기선 미사용. 체인 계약상 unit0 유지(fog 대칭).
Sampler2D uDepth;   // sceneFB depth 텍스처 (.r = 0~1 정규화 depth). unit1.

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
    output.pos = float4(input.aPos, 1.0);   // 이미 NDC 풀스크린 쿼드
    output.uv  = input.aUV;
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float d = uDepth.Sample(input.uv).r;   // 0~1 비선형(window space) 깊이

    if (uMaterial.uLinearize > 0.5)
    {
        // window depth(0~1) -> NDC z(-1~1) -> view-space z 복원 -> near..far 를 0~1 로 정규화.
        // (LearnOpenGL linearizeDepth 표준식. 표준 원근 투영 + glDepthRange[0,1] 가정.)
        float zNdc  = d * 2.0 - 1.0;
        float zView = (2.0 * uMaterial.uNear * uMaterial.uFar)
                    / (uMaterial.uFar + uMaterial.uNear - zNdc * (uMaterial.uFar - uMaterial.uNear));
        d = (zView - uMaterial.uNear) / (uMaterial.uFar - uMaterial.uNear); // 가까움=0(검정), 멂=1(흰색)
    }

    return float4(d, d, d, 1.0);
}
