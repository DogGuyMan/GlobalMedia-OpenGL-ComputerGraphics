#version 410 core

// _MyApp_ Phong (texture) FS
// LightUniformDispatcher 가 보내는 uniform schema 와 1:1.
// sentinel: viewPos — 이 uniform 이 없으면 dispatcher 가 라이팅 skip.

in vec3 vsNormal;
in vec3 vsPosition;
in vec2 vsTexCoord;

out vec4 fragColor;

// === Material ===
struct Material {
    sampler2D diffuse;    // unit 0 — model.cpp 의 "material.diffuse" 키와 일치
    sampler2D specular;   // unit 1 — model.cpp 의 "material.specular" 키와 일치
    float     shininess;
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
    vec3 position;
    vec3 attenuation;  // (Kc, Kl, Kq)
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3  position;
    vec3  direction;
    float cutoff;
    float outerCutoff;
    vec3  attenuation;  // (Kc, Kl, Kq)
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

// sentinel — LightUniformDispatcher 가 viewPos 존재 여부로 lighting 사용 판정
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
    return la * texture(material.diffuse, vsTexCoord).rgb;
}

vec3 CalcDiffuse(vec3 ld, vec3 N, vec3 L)
{
    return ld * max(dot(N, L), 0.0) * texture(material.diffuse, vsTexCoord).rgb;
}

vec3 CalcSpecular(vec3 ls, vec3 N, vec3 L, vec3 V)
{
    vec3  R  = reflect(-L, N);
    float rv = pow(max(dot(R, V), 0.0), material.shininess);
    return ls * rv * texture(material.specular, vsTexCoord).rgb;
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
    vec3  L    = normalize(l.position - vsPosition);
    float dist = length(l.position - vsPosition);
    float att  = CalcAttenuation(l.attenuation, dist);
    return (CalcAmbient(l.ambient)
          + CalcDiffuse (l.diffuse,  N, L)
          + CalcSpecular(l.specular, N, L, V)) * att;
}

vec3 CalcSpotLight(SpotLight l, vec3 N, vec3 V)
{
    vec3  L    = normalize(l.position - vsPosition);
    float dist = length(l.position - vsPosition);
    float att  = CalcAttenuation(l.attenuation, dist);
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
