// apps/_MyApp_/shaders_slang/phong_lighting.slang
// Phong 라이팅 SSOT 모듈 (Phase 2.5) - 모든 UBO lit 셰이더가 import 하는 단일 로직.
//
// 중복 제거: 구 phong_tex.fs / phong_albedo.fs / phong.slang 가 각자 복제하던
//   DirLight/PointLight/SpotLight struct + Calc* 6함수 + 광원 루프를 여기 1벌로 통합.
// 소비 셰이더는 ComputePhong(albedo, shininess, N, V, fragPos) 한 번만 호출.
//
// LightBlock UBO + std140 레이아웃은 src/render/light_ubo_uploader.cpp 의 C++ 미러와 byte 1:1.
//   (필드 순서/타입 변경 시 그 static_assert 가 빌드 차단.)

module phong_lighting;

static const int MAX_POINT_LIGHTS = 16;
static const int MAX_SPOT_LIGHTS  = 16;

// === 광원 struct (public - LightBlock 멤버로 노출) ===
public struct DirLightS
{
    float3 direction;
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

public struct PointLightS
{
    float3 position;
    float3 attenuation;   // (Kc, Kl, Kq)
    float3 ambient;
    float3 diffuse;
    float3 specular;
};

public struct SpotLightS
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

// === per-frame 공유 LightBlock UBO (LightUboUploader 소유 - S5b) ===
public struct LightBlock
{
    DirLightS   dirLight;
    PointLightS pointLights[MAX_POINT_LIGHTS];
    SpotLightS  spotLights[MAX_SPOT_LIGHTS];
    float3      viewPos;          // Phong specular 계산용 카메라 월드 위치
    int         dirLightEnabled;  // 0/1
    int         numPointLights;   // 활성 PointLight 수 (<= MAX_POINT_LIGHTS)
    int         numSpotLights;    // 활성 SpotLight 수  (<= MAX_SPOT_LIGHTS)
};
public ConstantBuffer<LightBlock> uLight;

// === Phong 헬퍼 (모듈 내부 - public 불요) ===
float CalcAttenuation(float3 k, float d)
{
    return 1.0 / dot(k, float3(1.0, d, d * d));
}

float CalcSoftEdge(float theta, float inner, float outer)
{
    return clamp((theta - outer) / (inner - outer), 0.0, 1.0);
}

float3 CalcAmbient(float3 la, float3 albedo)
{
    return la * albedo;
}

float3 CalcDiffuse(float3 ld, float3 N, float3 L, float3 albedo)
{
    return ld * max(dot(N, L), 0.0) * albedo;
}

float3 CalcSpecular(float3 ls, float3 N, float3 L, float3 V, float shininess)
{
    float3 R  = reflect(-L, N);
    float  rv = pow(max(dot(R, V), 0.0), shininess);
    return ls * rv;   // specular 는 표면 albedo 색에 독립 (하이라이트)
}

float3 CalcDirLight(DirLightS l, float3 N, float3 V, float3 albedo, float shininess)
{
    float3 L = normalize(-l.direction);
    return CalcAmbient(l.ambient, albedo)
         + CalcDiffuse(l.diffuse, N, L, albedo)
         + CalcSpecular(l.specular, N, L, V, shininess);
}

float3 CalcPointLight(PointLightS l, float3 N, float3 V, float3 fragPos, float3 albedo, float shininess)
{
    float3 L    = normalize(l.position - fragPos);
    float  dist = length(l.position - fragPos);
    float  att  = CalcAttenuation(l.attenuation, dist);
    return (CalcAmbient(l.ambient, albedo)
          + CalcDiffuse(l.diffuse, N, L, albedo)
          + CalcSpecular(l.specular, N, L, V, shininess)) * att;
}

float3 CalcSpotLight(SpotLightS l, float3 N, float3 V, float3 fragPos, float3 albedo, float shininess)
{
    float3 L    = normalize(l.position - fragPos);
    float  dist = length(l.position - fragPos);
    float  att  = CalcAttenuation(l.attenuation, dist);
    float  intn = CalcSoftEdge(dot(L, normalize(-l.direction)), l.cutoff, l.outerCutoff);
    return (CalcAmbient(l.ambient, albedo)
          + CalcDiffuse(l.diffuse, N, L, albedo)
          + CalcSpecular(l.specular, N, L, V, shininess)) * att * intn;
}

// === public 진입점 - 소비 셰이더가 한 줄로 전체 Phong 조명 계산 ===
// 소비 셰이더는 LightBlock 내부(viewPos 등)를 직접 만지지 않는다 - 캡슐화 (멤버 public 불요).
// V(프래그->카메라)는 모듈이 uLight.viewPos 에서 내부 도출.
// @param albedo    표면 반사색 (textured 셰이더는 sample 결과, albedo 셰이더는 baseColor.rgb)
// @param shininess specular 지수
// @param N         월드 노멀 (정규화)
// @param fragPos   월드 위치 (point/spot 거리 감쇠 + V 도출)
public float3 ComputePhong(float3 albedo, float shininess, float3 N, float3 fragPos)
{
    float3 V      = normalize(uLight.viewPos - fragPos);
    float3 result = float3(0.0, 0.0, 0.0);

    if (uLight.dirLightEnabled != 0)
        result += CalcDirLight(uLight.dirLight, N, V, albedo, shininess);

    for (int i = 0; i < uLight.numPointLights; ++i)
        result += CalcPointLight(uLight.pointLights[i], N, V, fragPos, albedo, shininess);

    for (int j = 0; j < uLight.numSpotLights; ++j)
        result += CalcSpotLight(uLight.spotLights[j], N, V, fragPos, albedo, shininess);

    return result;
}
