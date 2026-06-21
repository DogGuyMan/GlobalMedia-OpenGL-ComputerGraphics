// apps/_MyApp_/shaders_slang/matrix_skybox.slang
// 기존 matrix_skybox.vs + matrix_skybox.fs 등가 - Matrix 스타일 스크롤 skybox.
// VS: aPos(loc0)만 사용. uView 회전만(translation 제거)으로 무한 원경, gl_Position.xyww 로 depth=1.0.
//     -> Pass::Kind::Skybox (DepthFunc LEQUAL, cull FRONT, depthWrite off) 와 짝.
// FS: equirectangular UV + 글자 아틀라스(chars) + 노이즈(noise_tex) rain + u_time 스크롤.
// 블록: FrameBlock(view/proj) + MaterialBlock(u_time). uModel/DrawBlock 없음(mesh_pass 가 미존재 블록 자연 skip).
// 샘플러 chars/noise_tex 는 loose (이름 = Properties.Textures 키). 생성물: matrix_skybox.vs/.fs + .refl.json.
// Slang 변환: GLSL atan(y,x)->atan2, fract->frac, mod->fmod(피연산자 비음수라 동치), mix 없음.

struct FrameBlock
{
    float4x4 uView;
    float4x4 uProj;
};
ConstantBuffer<FrameBlock> uFrame;

struct MaterialBlock
{
    float u_time;   // Godot TIME 대체 (매 프레임 갱신)
};
ConstantBuffer<MaterialBlock> uMaterial;

Sampler2D chars;      // 글자 아틀라스 (unit 0)
Sampler2D noise_tex;  // 노이즈 (unit 1, REPEAT wrap)

struct VSIn
{
    [[vk::location(0)]] float3 aPos : POSITION;   // Box quad - 방향 벡터로도 사용
};

struct VSOut
{
    float4 pos : SV_Position;
    float3 dir : TEXCOORD0;   // 3D 방향 (구면 UV 변환 입력) - 유일 varying
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    VSOut output;
    output.dir = input.aPos;
    // view 의 translation 제거 (회전만) - 박스가 항상 카메라를 감싸 무한 원경.
    float3 rotated = mul((float3x3)uFrame.uView, input.aPos);
    float4 pos     = mul(uFrame.uProj, float4(rotated, 1.0));
    // depth 가 항상 1.0 (최대치) 유지하도록 z=w (xyww).
    output.pos = pos.xyww;
    return output;
}

// 격자/아틀라스/노이즈 상수 (구 GLSL const 동일).
static const float2 invAtan   = float2(0.15915494309, 0.31830988618);
static const float  COLS      = 256.0;
static const float  ROWS      = 128.0;
static const float  CHAR_W    = 64.0;
static const float  GAP       = 4.0;
static const float  ATLAS_W   = 684.0;   // = GAP + (CHAR_W + GAP) * 10
static const float  CHAR_CNT  = 10.0;
static const float  NOISE_SCALE  = 2.0;
static const float2 NOISE_OFFSET = float2(0.5, 0.0);

// 3D 방향 -> equirectangular 2D UV.
float2 getSphericalUV(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= invAtan;
    return uv;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float3 dir     = normalize(input.dir);
    float2 base_uv = getSphericalUV(dir);

    // 글자 격자 셀 분해
    float2 grid_uv = base_uv * float2(COLS, ROWS);
    float2 cellId  = floor(grid_uv);
    float2 inCell  = frac(grid_uv);

    // 디지트 선택 - 셀당 한 번 샘플 (셀 내부 일정).
    float g = noise_tex.Sample(cellId / float2(COLS, ROWS) * NOISE_SCALE + NOISE_OFFSET).g;
    float d = fmod(floor(g * CHAR_CNT) + floor(uMaterial.u_time * 5.0), CHAR_CNT);

    // 아틀라스 정확 매핑 - d 번째 64px 글자칸.
    float  cellLeftPx = GAP + d * (CHAR_W + GAP);
    float2 char_uv = float2((cellLeftPx + inCell.x * CHAR_W) / ATLAS_W, inCell.y);

    // 내리는 비 - 세로 그레디언트 + 노이즈 왜곡.
    float distortion = noise_tex.Sample(float2(cellId.x / COLS, 0.0) * NOISE_SCALE + NOISE_OFFSET).g;
    distortion = round(distortion * 10.0) / 10.0;

    float rain = cellId.y / ROWS;
    rain += round(uMaterial.u_time * 0.2 * ROWS) / ROWS;
    rain += distortion;
    rain = frac(rain);
    rain = round(rain * 16.0) / 16.0;
    rain = pow(rain, 3.0);
    rain *= 2.0;

    float3 color = chars.Sample(char_uv).rgb * rain * float3(0.0, 1.0, 0.0);
    return float4(color, 1.0);
}
