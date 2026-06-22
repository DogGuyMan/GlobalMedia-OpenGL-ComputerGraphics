// apps/_MyApp_/shaders_slang/healthbar.slang
// 기존 healthbar.vs + healthbar.fs 등가 - 머리 위 빌보드 체력바 (조각 분할 + fill + AA).
// 3블록 UBO(Frame/Draw/Material). 텍스처 없음 (절차적 조각 렌더). Pass::RenderQueue::Transparent (gap alpha=0).
// 생성물: healthbar.vs (vsMain) / healthbar.fs (fsMain) + healthbar.refl.json
//
// ★ 빌보드 카메라축 추출: 구 GLSL 은 uView 원소 직접 접근(uView[0][0] 등)으로 right/up 을 뽑았으나,
//   Slang->GLSL 행렬 인덱싱 + row_major 레이아웃의 컨벤션 모호성을 피하려 *transpose+mul* 로 추출한다.
//   cameraRight_world = (view->world)·(+x) = inverse-rotation·x = transpose(uView)·(1,0,0,0).
//   (orthonormal view 라 R^-1 = R^T. w=0 방향이라 translation 무영향.) mul 은 phong 에서 검증된 경로.

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

// std140: uColor @0(16) / uBgColor @16(16) / uFill @32 / uSegmentCount @36 / uSegmentSpacing @40 / uHeadOffset @44 - 총 48.
struct MaterialBlock
{
    float4 uColor;          // 채워진 조각 색
    float4 uBgColor;        // 빈 조각(트랙) 색
    float  uFill;           // 체력 비율 0..1 (HealthBarDriver 매 프레임 갱신)
    float  uSegmentCount;   // 조각 수
    float  uSegmentSpacing; // 조각 내 half-gap 비율
    float  uHeadOffset;     // 화면상 위(cameraUp)로 띄우는 거리 (월드 단위)
};
ConstantBuffer<MaterialBlock> uMaterial;

struct VSIn
{
    // Geometry::Plane XY quad. aPos(loc0) / aNormal(loc1 미사용) / aTexCoord(loc2).
    [[vk::location(0)]] float3 aPos      : POSITION;
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
    // 카메라 right/up (world) - transpose(view)·축 (inverse-rotation, w=0 방향).
    float3 cameraRight = mul(transpose(uFrame.uView), float4(1.0, 0.0, 0.0, 0.0)).xyz;
    float3 cameraUp    = mul(transpose(uFrame.uView), float4(0.0, 1.0, 0.0, 0.0)).xyz;

    // uModel 에서 center + size 추출 (mul 기반 - 원소 인덱싱 회피).
    float3 center = mul(uDraw.uModel, float4(0.0, 0.0, 0.0, 1.0)).xyz;
    float  sx     = length(mul(uDraw.uModel, float4(1.0, 0.0, 0.0, 0.0)).xyz);  // Scale.x -> 바 가로폭
    float  sy     = length(mul(uDraw.uModel, float4(0.0, 1.0, 0.0, 0.0)).xyz);  // Scale.y -> 바 세로높이

    // 머리 위 앵커: 화면상 cameraUp 방향 오프셋 (카메라각 독립).
    float3 anchor   = center + cameraUp * uMaterial.uHeadOffset;
    float3 worldPos = anchor + cameraRight * input.aPos.x * sx + cameraUp * input.aPos.y * sy;

    VSOut output;
    output.uv  = input.aTexCoord;
    output.pos = mul(uFrame.uProj, mul(uFrame.uView, float4(worldPos, 1.0)));
    return output;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float u = clamp(input.uv.x, 0.0, 1.0);
    float N = max(uMaterial.uSegmentCount, 1.0);

    // 현재 조각 내부 좌표 + 조각 경계까지 거리
    float f    = frac(u * N);
    float edge = min(f, 1.0 - f);

    // fwidth 안티에일리어싱
    float aaSeg = fwidth(u * N);
    float aaU   = fwidth(u);

    // 조각 몸체 = 1, 조각 사이 간격(gap) = 0 (gap 은 투명)
    float body = smoothstep(uMaterial.uSegmentSpacing, uMaterial.uSegmentSpacing + aaSeg, edge);

    // 좌->우 채움: u < uFill 채워짐 (우측부터 비워짐), 경계 AA
    float fill = 1.0 - smoothstep(uMaterial.uFill - aaU, uMaterial.uFill + aaU, u);

    // 채워진 조각=uColor, 빈 조각=uBgColor. gap 은 alpha*body 로 투명.
    float3 rgb = lerp(uMaterial.uBgColor.rgb, uMaterial.uColor.rgb, fill);
    float  a   = lerp(uMaterial.uBgColor.a, uMaterial.uColor.a, fill) * body;

    return float4(rgb, a);
}
