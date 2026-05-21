#version 410 core

// migrate_demo Phong (texture) FS — DirLight + 2 PointLight + 1 SpotLight 통합.
// 레퍼런스 OpenGL-With-CMake/resources/shader/lighting.fs 와 동일 schema (sampler2D material).
// SJH RenderSystem 의 light uniform schema 와 1:1 (program_uniforms.cpp 의 SFX_* suffix 컨벤션).

in vec3 vsNormal;
in vec3 vsPosition;
in vec2 vsTexCoord;

out vec4 fragColor;

// === Material — sampler2D 기반 (diffuse / specular 텍스처 + shininess) ===
struct Material {
    sampler2D diffuse;
    sampler2D specular;
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
    vec3 attenuation;   // (Kc, Kl, Kq)
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutoff;       // cos(inner)
    float outerCutoff;  // cos(outer)
    vec3 attenuation;   // (Kc, Kl, Kq)
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NUM_POINT_LIGHTS 2

uniform DirLight   dirLight;
uniform PointLight pointLights[NUM_POINT_LIGHTS];
uniform SpotLight  spotLight;

uniform int dirLightEnabled;
uniform int pointLightsEnabled[NUM_POINT_LIGHTS];
uniform int spotLightEnabled;

uniform vec3 viewPos;

// === Phong 헬퍼 ===
float CalcAttenuation(vec3 attenuationCoeff, float dist)
{
    vec3 distPoly = vec3(1.0, dist, dist * dist);
    return 1.0 / dot(distPoly, attenuationCoeff);
}

float CalcSoftEdge(float theta, float innerCutoff, float outerCutoff)
{
    float epsilon = innerCutoff - outerCutoff;
    return clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);
}

vec3 CalcAmbient(vec3 lightAmbient)
{
    vec3 diffuseTexColor = texture(material.diffuse, vsTexCoord).rgb;
    return lightAmbient * diffuseTexColor;
}

vec3 CalcDiffuse(vec3 lightDiffuse, vec3 N, vec3 L)
{
    vec3 diffuseTexColor = texture(material.diffuse, vsTexCoord).rgb;
    float NdotL = max(dot(N, L), 0.0);
    return lightDiffuse * (NdotL * diffuseTexColor);
}

vec3 CalcSpecular(vec3 lightSpecular, vec3 N, vec3 L, vec3 V)
{
    vec3 specularTexColor = texture(material.specular, vsTexCoord).rgb;
    vec3 R = reflect(-L, N);
    float RV = pow(max(dot(R, V), 0.0), material.shininess);
    return lightSpecular * (RV * specularTexColor);
}

vec3 CalcDirLight(DirLight light, vec3 pixelNorm, vec3 viewDir)
{
    vec3 lightDir = normalize(-light.direction);
    vec3 ambient  = CalcAmbient (light.ambient);
    vec3 diffuse  = CalcDiffuse (light.diffuse,  pixelNorm, lightDir);
    vec3 specular = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return ambient + diffuse + specular;
}

vec3 CalcPointLight(PointLight light, vec3 pixelNorm, vec3 viewDir)
{
    vec3  lightDir    = normalize(light.position - vsPosition);
    float dist        = length(light.position - vsPosition);
    float attenuation = CalcAttenuation(light.attenuation, dist);
    vec3  ambient     = CalcAmbient (light.ambient);
    vec3  diffuse     = CalcDiffuse (light.diffuse,  pixelNorm, lightDir);
    vec3  specular    = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(SpotLight light, vec3 pixelNorm, vec3 viewDir)
{
    vec3  lightDir    = normalize(light.position - vsPosition);
    float dist        = length(light.position - vsPosition);
    float attenuation = CalcAttenuation(light.attenuation, dist);
    float theta       = dot(lightDir, normalize(-light.direction));
    float intensity   = CalcSoftEdge(theta, light.cutoff, light.outerCutoff);
    vec3  ambient     = CalcAmbient (light.ambient);
    vec3  diffuse     = CalcDiffuse (light.diffuse,  pixelNorm, lightDir);
    vec3  specular    = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return (ambient + diffuse + specular) * attenuation * intensity;
}

void main()
{
    vec3 pixelNorm = normalize(vsNormal);
    vec3 viewDir   = normalize(viewPos - vsPosition);
    vec3 result    = vec3(0.0);

    if (dirLightEnabled != 0)
        result += CalcDirLight(dirLight, pixelNorm, viewDir);

    for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
        if (pointLightsEnabled[i] != 0)
            result += CalcPointLight(pointLights[i], pixelNorm, viewDir);
    }

    if (spotLightEnabled != 0)
        result += CalcSpotLight(spotLight, pixelNorm, viewDir);

    fragColor = vec4(result, 1.0);
}
