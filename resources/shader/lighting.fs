#version 330 core

in vec3 vsNormal;
in vec3 vsPosition;
in vec2 vsTexCoord;

// Rasterization 과정을 거쳐 픽셀별로 할당된 vertex shader의 출력값이 입력됨
out vec4 fragColor;

// =========================================================================
// Material — diffuse / specular 텍스처 + shininess
// =========================================================================
struct Material {
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};
uniform Material material;

// =========================================================================
// Light Caster 타입 — Directional / Point / Spot
//  CPU(C++ light.h) 의 DirectionalLight / PointLight / SpotLight 클래스와 1:1
// =========================================================================
struct DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    vec3 attenuation;        // (Kc, Kl, Kq) — GetAttenuationCoeff(distance) 결과
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
    vec3 direction;
    float cutoff;            // cos(inner cone half-angle)
    float outerCutoff;       // cos(outer cone half-angle) — soft edge 끝
    vec3 attenuation;        // (Kc, Kl, Kq)
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

#define NUM_POINT_LIGHTS 2

uniform DirLight dirLight;
uniform PointLight pointLights[NUM_POINT_LIGHTS];   // GLSL은 동적 배열 불가 — 컴파일 타임 상수
uniform SpotLight spotLight;

// === Light 활성/비활성 플래그 — runtime toggle (GLSL bool 은 송신 시 int 사용) ===
// 0 = skip (해당 광원 계산·누적 생략), 1 = active. CPU 가 매 프레임 송신.
// 셰이더가 receive 안 한 경우 GL default 0 -> 의도치 않게 전부 검정 화면이 될 수 있으므로
// CPU 측은 반드시 매 프레임 3개 모두 SetInt 로 보낼 것 (context.cpp Render).
uniform int dirLightEnabled;
uniform int pointLightsEnabled[NUM_POINT_LIGHTS];
uniform int spotLightEnabled;

uniform vec3 viewPos;

// =========================================================================
// 공통 헬퍼 — 광원 타입 무관 (Phong 3항 + 거리감쇠 + soft edge)
// =========================================================================

// 거리 감쇠 :  1 / (Kc + Kl·d + Kq·d²)
float CalcAttenuation(vec3 attenuationCoeff, float dist) {
    vec3 distPoly = vec3(1.0, dist, dist * dist);   // (1, d, d²)
    //  distPoly.x * attenuationCoeff.x  => 1.0 * Kc
    //  distPoly.y * attenuationCoeff.y  => d   * Kl
    //  distPoly.z * attenuationCoeff.z  => d²  * Kq
    return 1.0 / dot(distPoly, attenuationCoeff);
}

// 스포트 콘 soft edge : inner 안쪽 1, outer 바깥 0, 사이는 선형 페이드
float CalcSoftEdge(float theta, float innerCutoff, float outerCutoff) {
    if (theta > innerCutoff) return 1.0;                                // 안쪽 — 100% 밝기
    if (theta < outerCutoff) return 0.0;                                // 바깥 — 0% 밝기
    return (theta - outerCutoff) / (innerCutoff - outerCutoff);         // 페이드 구간 (0~1)
}

// Phong 3항 — 광원의 채널 색만 받아 픽셀별 기여도 산출
vec3 CalcAmbient(vec3 lightAmbient) {
    vec3 diffuseTexColor = texture(material.diffuse, vsTexCoord).rgb;
    return lightAmbient * diffuseTexColor;
}

vec3 CalcDiffuse(vec3 lightDiffuse, vec3 pixelNorm, vec3 lightDir) {
    vec3 diffuseTexColor = texture(material.diffuse, vsTexCoord).rgb;
    float diff = max(dot(pixelNorm, lightDir), 0.0);
    return lightDiffuse * diff * diffuseTexColor;
}

vec3 CalcSpecular(vec3 lightSpecular, vec3 pixelNorm, vec3 lightDir, vec3 viewDir) {
    vec3 specularTexColor = texture(material.specular, vsTexCoord).rgb;
    vec3 reflectDir = reflect(-lightDir, pixelNorm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    return lightSpecular * spec * specularTexColor;
}

// =========================================================================
// Light Caster 디스패처 — 타입별로 lightDir / 감쇠 / 콘 cutoff 처리 분기
// =========================================================================

// 방향광 — 모든 표면이 같은 lightDir, 감쇠 없음 (태양처럼 무한 거리)
vec3 CalcDirLight(DirLight light, vec3 pixelNorm, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);    // 표면 -> 광원 방향
    vec3 ambient  = CalcAmbient(light.ambient);
    vec3 diffuse  = CalcDiffuse(light.diffuse, pixelNorm, lightDir);
    vec3 specular = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return ambient + diffuse + specular;
}

// 점광원 — lightDir 가 픽셀마다 다름, 거리감쇠 적용 (전구처럼)
vec3 CalcPointLight(PointLight light, vec3 pixelNorm, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - vsPosition);
    float dist = length(light.position - vsPosition);
    float attenuation = CalcAttenuation(light.attenuation, dist);
    vec3 ambient  = CalcAmbient(light.ambient);
    vec3 diffuse  = CalcDiffuse(light.diffuse, pixelNorm, lightDir);
    vec3 specular = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return (ambient + diffuse + specular) * attenuation;
}

// 스포트라이트 — 점광원 + 콘 cutoff (손전등처럼)
vec3 CalcSpotLight(SpotLight light, vec3 pixelNorm, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - vsPosition);
    float dist = length(light.position - vsPosition);
    float attenuation = CalcAttenuation(light.attenuation, dist);
    // 콘 축(-direction)과 표면->광원 방향(lightDir) 사이 각도의 cos
    float theta = dot(lightDir, normalize(-light.direction));
    float intensity = CalcSoftEdge(theta, light.cutoff, light.outerCutoff);
    vec3 ambient  = CalcAmbient(light.ambient);
    vec3 diffuse  = CalcDiffuse(light.diffuse, pixelNorm, lightDir);
    vec3 specular = CalcSpecular(light.specular, pixelNorm, lightDir, viewDir);
    return (ambient + diffuse + specular) * attenuation * intensity;
}

// =========================================================================
// 엔트리 — 3 종류 light caster 누적
// =========================================================================
void main() {
    // 보간된 normal 은 fragment shader 진입 후 다시 정규화 필요 (선형 보간 결과 길이 1 보장 안 됨)
    vec3 pixelNorm = normalize(vsNormal);
    vec3 viewDir = normalize(viewPos - vsPosition);
    vec3 result = vec3(0.0);

    // 1) 방향광 — enabled 플래그가 켜져 있을 때만 누적 (전역 평행광 1개)
    if (dirLightEnabled != 0)
        result += CalcDirLight(dirLight, pixelNorm, viewDir);

    // 2) 점광원들 — 각 슬롯 독립 토글. pointLightsEnabled[i] == 0 이면 그 슬롯 skip.
    //    덕분에 "0 번 점광원만 살리고 1 번은 끈다" 같은 부분 활성 시나리오 지원.
    for (int i = 0; i < NUM_POINT_LIGHTS; ++i) {
        if (pointLightsEnabled[i] != 0)
            result += CalcPointLight(pointLights[i], pixelNorm, viewDir);
    }

    // 3) 스포트라이트 (1개)
    if (spotLightEnabled != 0)
        result += CalcSpotLight(spotLight, pixelNorm, viewDir);

    fragColor = vec4(result, 1.0);

    //Depth visualization
    // fragColor = vec4(vec3(gl_FragCoord.z), 1.0);
}
