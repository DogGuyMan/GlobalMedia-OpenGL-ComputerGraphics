#version 410 core

// _MyApp_ Phong (albedo color) FS — sampler2D 없이 material.albedo (vec3) 로 diffuse 계산.
// FBX 에 임베디드 텍스처가 없는 모델(PCB 등)에 사용.
// VS 는 phong_tex.vs 공유 (동일 attribute layout / matrix uniform).
//
// sentinel: viewPos — LightUniformDispatcher 가 이 uniform 존재 여부로 lighting 사용 판정.

in vec3 vsNormal;
in vec3 vsPosition;
in vec2 vsTexCoord;

out vec4 fragColor;

// === Material ===
struct Material {
    vec3  albedo;      // model.cpp 가 aiColor_Diffuse 에서 추출한 Albedo 색
    float shininess;
};
uniform Material material;

// === Light 타입 ===
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3  position;
    vec3  attenuation;  // (Kc, Kl, Kq)
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

struct SpotLight {
    vec3  position;
    vec3  direction;
    float cutoff;
    float outerCutoff;
    vec3  attenuation;
    vec3  ambient;
    vec3  diffuse;
    vec3  specular;
};

#define MAX_POINT_LIGHTS 16
#define MAX_SPOT_LIGHTS  16

uniform DirLight   dirLight;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
uniform SpotLight  spotLights [MAX_SPOT_LIGHTS];

uniform int dirLightEnabled;
uniform int pointLightsEnabled[MAX_POINT_LIGHTS];
uniform int spotLightsEnabled [MAX_SPOT_LIGHTS];

uniform vec3 viewPos;

// === Phong 헬퍼 ===

float CalcAttenuation(vec3 k, float d)
{
    return 1.0 / dot(k, vec3(1.0, d, d * d));
}

float CalcSoftEdge(float theta, float inner, float outer)
{
    return clamp((theta - outer) / (inner - outer), 0.0, 1.0);
}

vec3 CalcAmbient(vec3 la)
{
    return la * material.albedo;
}

vec3 CalcDiffuse(vec3 ld, vec3 N, vec3 L)
{
    return ld * max(dot(N, L), 0.0) * material.albedo;
}

vec3 CalcSpecular(vec3 ls, vec3 N, vec3 L, vec3 V)
{
    vec3  R  = reflect(-L, N);
    float rv = pow(max(dot(R, V), 0.0), material.shininess);
    return ls * rv;  // specular 는 표면 albedo 색에 독립 (하이라이트)
}

vec3 CalcDirLight(DirLight l, vec3 N, vec3 V)
{
    vec3 L = normalize(-l.direction);
    return CalcAmbient(l.ambient)
         + CalcDiffuse (l.diffuse,  N, L)
         + CalcSpecular(l.specular, N, L, V);
}

vec3 CalcPointLight(PointLight l, vec3 N, vec3 V)
{
    vec3  L   = normalize(l.position - vsPosition);
    float d   = length(l.position - vsPosition);
    float att = CalcAttenuation(l.attenuation, d);
    return (CalcAmbient(l.ambient)
          + CalcDiffuse (l.diffuse,  N, L)
          + CalcSpecular(l.specular, N, L, V)) * att;
}

vec3 CalcSpotLight(SpotLight l, vec3 N, vec3 V)
{
    vec3  L    = normalize(l.position - vsPosition);
    float d    = length(l.position - vsPosition);
    float att  = CalcAttenuation(l.attenuation, d);
    float intn = CalcSoftEdge(dot(L, normalize(-l.direction)), l.cutoff, l.outerCutoff);
    return (CalcAmbient(l.ambient)
          + CalcDiffuse (l.diffuse,  N, L)
          + CalcSpecular(l.specular, N, L, V)) * att * intn;
}

void main()
{
    vec3 N      = normalize(vsNormal);
    vec3 V      = normalize(viewPos - vsPosition);
    vec3 result = vec3(0.0);

    if (dirLightEnabled != 0)
        result += CalcDirLight(dirLight, N, V);

    for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
        if (pointLightsEnabled[i] != 0)
            result += CalcPointLight(pointLights[i], N, V);

    for (int i = 0; i < MAX_SPOT_LIGHTS; ++i)
        if (spotLightsEnabled[i] != 0)
            result += CalcSpotLight(spotLights[i], N, V);

    fragColor = vec4(result, 1.0);
}
