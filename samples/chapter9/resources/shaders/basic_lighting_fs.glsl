#version 410 core
// #version 430 core

in vec3 vsPosition;
in vec2 vsTexCoord;
in vec3 vsNormal;
in vec3 vsColor;

out vec4 fragColor;

struct Material {
        sampler2D diffuse;
        sampler2D specular;
        float shininess;
};

struct Light {
        vec3 position;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
};

struct DirLight {
        vec3 direction;
        vec3 ambient, diffuse, specular;
};

struct PointLight {
        vec3 position;
        float c1, c2;
        vec3 ambient, diffuse, specular;
};

struct SpotLight {
        vec3 position;
        vec3 direction;
        // cutOff cosϕ inner : 안쪽은 100% 밝기
        // outerCutOff cosγ outer : 바깥은 0% 밝기
        float cutOff, outerCutOff;
        float c1, c2;
        vec3 ambient, diffuse, specular;
};

#define NUM_POINT_LIGHTS 2

uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLights[NUM_POINT_LIGHTS]; // GLSL은 동적 배열 불가
uniform SpotLight spotLight;
// uniform Light light;

uniform vec3 viewPos;
uniform vec3 objectColor;
uniform int uObjectID; // C++ 에서 VAO/드로우콜마다 다른 정수 전달 (0,1,2,…)
uniform int uRenderMode; // 0=Light(1st pass), 1=Depth, 2=Outline(2nd pass, StencilResult 색)

float CalcAttenuation(vec2 lightCoeff, float d);
float CalcSoftEdge(float theta, float phi, float gamma);
vec3 CalcAmbient(vec3 lightAmbient);
vec3 CalcDiffuse(vec3 lightDiffuse, vec3 normal, vec3 lightDir);
vec3 CalcSpecular(vec3 lightSpecular, vec3 normal, vec3 lightDir, vec3 viewDir);
vec3 CalcPhongLight(Light light, vec3 normal, vec3 viewDir);
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

vec4 LightResult() {
        vec3 norm = normalize(vsNormal); // 보간된 노말은 한 번만 정규화해서 각 Calc* 에 전달
        vec3 viewDir = normalize(viewPos - vsPosition);
        vec3 fragPos = vsPosition;
        fragColor = vec4(vsColor, 1.0f);
        fragColor *= vec4(objectColor, 1.0f);

        // 1) 방향광 — 전역 평행광
        vec3 result = CalcDirLight(dirLight, norm, viewDir);

        // 2) 점광원들
        for (int i = 0; i < NUM_POINT_LIGHTS; ++i)
                result += CalcPointLight(pointLights[i], norm, fragPos, viewDir);

        // 3) 스포트라이트
        result += CalcSpotLight(spotLight, norm, fragPos, viewDir);
        return vec4(result, 1.0);
}

vec4 DepthResult();
vec4 StencilResult();

void main()
{
        if (uRenderMode == 2) fragColor = StencilResult(); // outline pass — 외곽 색
        else if (uRenderMode == 1) fragColor = DepthResult(); // depth 시각화
        else fragColor = LightResult(); // 기본 (1st pass)
}

/*
        | 커버 Distance | $c_1$ | $c_2$ |
        |--------------|-------|-------|
        | 7            | 0.7   | 1.8   |
        | 20           | 0.22  | 0.2   |
        | 50           | 0.09  | 0.032 |
        | 100          | 0.045 | 0.0075|
        | 200          | 0.022 | 0.0019|
        | 600          | 0.007 | 0.0002|
        | 3250         | 0.0014| 0.000007 |
        */
float CalcAttenuation(vec2
        lightCoeff, float
        d)
{
        float c1 = lightCoeff.x;
        float c2 = lightCoeff.y;
        return 1.0 / (1.0 + c1 * d + c2 * d * d);
}

float CalcSoftEdge(float theta, float phi, float gamma) {
        float intensity = 0.0;
        if (theta > phi) // cutOff cosϕ
                intensity = 1.0; // inner : 안쪽은 100% 밝기
        else if (theta < gamma) // outerCutOff
                intensity = 0.0; // cosγ outer : 바깥은 0% 밝기
        else // 페이드 구간 (이 범위에선 0~1 보장)
                intensity = (theta - gamma) / (phi - gamma);
        return intensity;
}

vec3 CalcAmbient(vec3 lightAmbient) {
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord));
        vec3 ambient = lightAmbient * diffuseTextureColor;
        return ambient;
}

vec3 CalcDiffuse(vec3 lightDiffuse, vec3 normal, vec3 lightDir) {
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord));
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = lightDiffuse * diff * diffuseTextureColor;
        return diffuse;
}

vec3 CalcSpecular(vec3 lightSpecular, vec3 normal, vec3 lightDir, vec3 viewDir) {
        vec3 specularTextureColor = vec3(texture(material.specular, vsTexCoord));
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = lightSpecular * spec * specularTextureColor;
        return specular;
}

vec3 CalcPhongLight(Light light, vec3 normal, vec3 viewDir) {
        vec3 lightDir = normalize(light.position - vsPosition); // !
        vec3 ambient = CalcAmbient(light.ambient);
        vec3 diffuse = CalcDiffuse(light.diffuse, normal, lightDir);
        vec3 specular = CalcSpecular(light.specular, normal, lightDir, viewDir);
        return (ambient + diffuse + specular);
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
        vec3 lightDir = normalize(-light.direction); // !
        vec3 ambient = CalcAmbient(light.ambient);
        vec3 diffuse = CalcDiffuse(light.diffuse, normal, lightDir);
        vec3 specular = CalcSpecular(light.specular, normal, lightDir, viewDir);
        return (ambient + diffuse + specular); // attenuation 없음
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
        vec3 lightDir = normalize(light.position - fragPos); // !
        vec3 ambient = CalcAmbient(light.ambient);
        vec3 diffuse = CalcDiffuse(light.diffuse, normal, lightDir);
        vec3 specular = CalcSpecular(light.specular, normal, lightDir, viewDir);

        // attenuation (감쇠) 공식
        float dist = length(light.position - fragPos);
        float attenuation = CalcAttenuation(vec2(light.c1, light.c2), dist);

        return (ambient + diffuse + specular) * attenuation;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
        vec3 lightDir = normalize(light.position - fragPos); // !
        vec3 ambient = CalcAmbient(light.ambient);
        vec3 diffuse = CalcDiffuse(light.diffuse, normal, lightDir);
        vec3 specular = CalcSpecular(light.specular, normal, lightDir, viewDir);

        // 소프트 에지
        float theta = dot(lightDir, normalize(-light.direction));
        float phi = light.cutOff;
        float gamma = light.outerCutOff;
        float intensity = CalcSoftEdge(theta, phi, gamma);

        // attenuation (감쇠) 공식
        float dist = length(light.position - fragPos);
        float attenuation = CalcAttenuation(vec2(light.c1, light.c2), dist);

        // 세 요소 모두에 attenuation × intensity 곱
        ambient *= attenuation * intensity;
        diffuse *= attenuation * intensity;
        specular *= attenuation * intensity;
        return (ambient + diffuse + specular);
}

vec4 DepthResult()
{
        float nearPlane = 0.1;
        float farPlane = 1000.0;
        float zDepth = gl_FragCoord.z;
        float z = zDepth * 2.0 - 1.0;
        float linearDepth = (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));

        // 보고 싶은 거리 범위를 [0,1] 로 늘려 saturate — 평활화 흉내
        float minDist = 1.0; // 이보다 가까운 픽셀 -> 1.0 (흰색)
        float maxDist = 5.0; // 이보다 먼 픽셀     -> 0.0 (검정)
        float t = clamp((linearDepth - minDist) / (maxDist - minDist), 0.0, 1.0); // saturate
        float res = 1.0 - t; // 가까울수록 밝게
        return vec4(res, res, res, 1.0);
}

// VAO 별 고유 색상 — uObjectID 를 stencil-ref 처럼 사용해 팔레트에서 색을 뽑는다.
// C++ 쪽에서 진짜 stencil 도 같이 채우면 외곽선/마스킹용으로 재활용 가능.
vec4 StencilResult()
{
        const vec3 palette[8] = vec3[8](
                        vec3(1.0, 0.25, 0.25), // 0 : 빨강
                        vec3(0.25, 1.0, 0.35), // 1 : 초록
                        vec3(0.30, 0.45, 1.0), // 2 : 파랑
                        vec3(1.0, 0.95, 0.30), // 3 : 노랑
                        vec3(1.0, 0.40, 1.0), // 4 : 마젠타
                        vec3(0.25, 1.0, 1.0), // 5 : 시안
                        vec3(1.0, 0.55, 0.20), // 6 : 주황
                        vec3(0.65, 0.40, 1.0) // 7 : 보라
                );
        int idx = (uObjectID % 8 + 8) % 8; // 음수 ID 안전 처리
        return vec4(palette[idx], 1.0);
}
