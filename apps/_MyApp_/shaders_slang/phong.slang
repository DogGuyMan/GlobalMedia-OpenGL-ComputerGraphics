// apps/_MyApp_/shaders_slang/phong.slang
// phong_tex.{vs,fs} 의 Slang/UBO 등가 (Phase 2.5 - P25-T1).
//
// 블록 분할(D11 확장):
//   FrameBlock    (per-frame, mesh_pass 갱신)            : uView / uProj  <- simple.slang 과 동일 레이아웃 (재사용)
//   DrawBlock     (per-draw,  mesh_pass 갱신)            : uModel
//   MaterialBlock (per-material)                         : uShininess     <- 텍스처는 UBO 불가, loose sampler 유지
//   LightBlock    (per-frame *공유*, dispatcher 소유 S5b): dir + point[16] + spot[16] + viewPos + counts
//
// 광원 개수는 enabled 배열(std140 16B*N 낭비) 대신 count 기반(numPointLights/numSpotLights).
// viewPos 는 LightBlock 에 둔다 - dispatcher 가 소유/갱신하는 per-frame 광원 데이터의 일부.
// normal 행렬은 PoC 단순화로 (float3x3)uModel (균등 스케일 가정) - 비균등 스케일 시 T6 에서 정밀화.
// 생성물: phong.vs (vsMain) / phong.fs (fsMain) + phong.refl.json (std140 offset 메타).

static const int MAX_POINT_LIGHTS = 16;
static const int MAX_SPOT_LIGHTS  = 16;

// === per-frame / per-draw / per-material (mesh_pass 갱신 - simple.slang 과 동형) ===
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
    float uShininess;
};
ConstantBuffer<MaterialBlock> uMaterial;

// === per-frame 공유 LightBlock (dispatcher 소유 - S5b) ===
struct DirLightS
{
    float3 direction;
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct PointLightS
{
    float3 position;
    float3 attenuation;   // (Kc, Kl, Kq)
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct SpotLightS
{
    float3 position;
    float3 direction;
    float  cutoff;
    float  outerCutoff;
    float3 attenuation;   // (Kc, Kl, Kq)
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

struct LightBlock
{
    DirLightS   dirLight;
    PointLightS pointLights[MAX_POINT_LIGHTS];
    SpotLightS  spotLights[MAX_SPOT_LIGHTS];
    float3      viewPos;          // Phong specular 계산용 카메라 월드 위치
    int         dirLightEnabled;  // 0/1
    int         numPointLights;   // 활성 PointLight 수 (<= MAX_POINT_LIGHTS)
    int         numSpotLights;    // 활성 SpotLight 수  (<= MAX_SPOT_LIGHTS)
};
ConstantBuffer<LightBlock> uLight;

// === 텍스처 - combined sampler (UBO 불가, loose uniform sampler2D 로 산출) ===
Sampler2D uDiffuse;
Sampler2D uSpecular;

// === 정점 IO ===
struct VSIn
{
    float3 aPos      : POSITION;   // loc 0 - SJH::Vertex 레이아웃
    float3 aNormal   : NORMAL;     // loc 1
    float2 aTexCoord : TEXCOORD0;  // loc 2
};

struct VSOut
{
    float4 pos     : SV_Position;
    float3 wPos    : POSITION0;    // world position
    float3 wNormal : NORMAL0;      // world normal
    float2 uv      : TEXCOORD0;
};

[shader("vertex")]
VSOut vsMain(VSIn input)
{
    VSOut o;
    float4 world = mul(uDraw.uModel, float4(input.aPos, 1.0));
    o.wPos    = world.xyz;
    o.wNormal = mul((float3x3)uDraw.uModel, input.aNormal);  // PoC 단순화 (균등 스케일 가정)
    o.uv      = input.aTexCoord;
    o.pos     = mul(uFrame.uProj, mul(uFrame.uView, world));
    return o;
}

// === Phong 헬퍼 (phong_tex.fs 와 1:1) ===
float3 CalcAmbient(float3 la, float2 uv)
{
    return la * uDiffuse.Sample(uv).rgb;
}

float3 CalcDiffuse(float3 ld, float3 N, float3 L, float2 uv)
{
    return ld * max(dot(N, L), 0.0) * uDiffuse.Sample(uv).rgb;
}

float3 CalcSpecular(float3 ls, float3 N, float3 L, float3 V, float2 uv)
{
    float3 R  = reflect(-L, N);
    float  rv = pow(max(dot(R, V), 0.0), uMaterial.uShininess);
    return ls * rv * uSpecular.Sample(uv).rgb;
}

float CalcAttenuation(float3 k, float d)
{
    return 1.0 / dot(k, float3(1.0, d, d * d));
}

float CalcSoftEdge(float theta, float inner, float outer)
{
    return clamp((theta - outer) / (inner - outer), 0.0, 1.0);
}

float3 CalcDirLight(DirLightS l, float3 N, float3 V, float2 uv)
{
    float3 L = normalize(-l.direction);
    return CalcAmbient(l.ambient, uv)
         + CalcDiffuse(l.diffuse, N, L, uv)
         + CalcSpecular(l.specular, N, L, V, uv);
}

float3 CalcPointLight(PointLightS l, float3 N, float3 V, float3 fragPos, float2 uv)
{
    float3 L    = normalize(l.position - fragPos);
    float  dist = length(l.position - fragPos);
    float  att  = CalcAttenuation(l.attenuation, dist);
    return (CalcAmbient(l.ambient, uv)
          + CalcDiffuse(l.diffuse, N, L, uv)
          + CalcSpecular(l.specular, N, L, V, uv)) * att;
}

float3 CalcSpotLight(SpotLightS l, float3 N, float3 V, float3 fragPos, float2 uv)
{
    float3 L    = normalize(l.position - fragPos);
    float  dist = length(l.position - fragPos);
    float  att  = CalcAttenuation(l.attenuation, dist);
    float  intn = CalcSoftEdge(dot(L, normalize(-l.direction)), l.cutoff, l.outerCutoff);
    return (CalcAmbient(l.ambient, uv)
          + CalcDiffuse(l.diffuse, N, L, uv)
          + CalcSpecular(l.specular, N, L, V, uv)) * att * intn;
}

[shader("fragment")]
float4 fsMain(VSOut input) : SV_Target
{
    float3 N      = normalize(input.wNormal);
    float3 V      = normalize(uLight.viewPos - input.wPos);
    float3 result = float3(0.0, 0.0, 0.0);

    if (uLight.dirLightEnabled != 0)
        result += CalcDirLight(uLight.dirLight, N, V, input.uv);

    for (int i = 0; i < uLight.numPointLights; ++i)
        result += CalcPointLight(uLight.pointLights[i], N, V, input.wPos, input.uv);

    for (int j = 0; j < uLight.numSpotLights; ++j)
        result += CalcSpotLight(uLight.spotLights[j], N, V, input.wPos, input.uv);

    return float4(result, 1.0);
}
