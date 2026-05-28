#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene; // SP4 컨벤션: 렌더링된 원본 씬 컬러
uniform sampler2D uDepth; // Depth Map

// 역 투영 행렬 (NDC -> View Space 좌표 복원용)
uniform mat4 uInverseProjection;

// Fog 파라미터
uniform vec3 uFogColor = vec3(0.5, 0.6, 0.7);
uniform float uFogDensity = 0.05;
uniform float uFogStart = 5.0;
uniform float uFogEnd = 50.0;
uniform int uFogMode = 2; // 0: Linear, 1: Exp, 2: Exp2

// ==========================================
// glsl-fog 레퍼런스 함수
// https://github.com/hughsk/glsl-fog
// ==========================================

// 1. Linear Fog
float fogFactorLinear(const float dist, const float start, const float end) {
        return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}

// 2. Exponential Fog
float fogFactorExp(const float dist, const float density) {
        return 1.0 - clamp(exp(-density * dist), 0.0, 1.0);
}

// 3. Exponential Squared Fog
float fogFactorExp2(const float dist, const float density) {
        const float LOG2 = -1.442695;
        float d = density * dist;
        return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
        // 1. Scene 색상 및 화면 Depth 가져오기
        vec3 sceneColor = texture(uScene, vUV).rgb;
        float depthVal = texture(uDepth, vUV).r;

        // 2. Depth Map을 사용하여 View Space 좌표로 복원 (Inverse Projection)
        vec4 ndc = vec4(vUV * 2.0 - 1.0, depthVal * 2.0 - 1.0, 1.0);
        vec4 viewPos = uInverseProjection * ndc;
        viewPos /= viewPos.w; // Perspective Divide (w로 나누어 실제 좌표 획득)

        // 카메라로부터 픽셀까지의 유클리디안 거리 계산
        float dist = length(viewPos.xyz);

        // 3. 거리와 설정에 따른 Fog Factor 계산
        float fogAmount = 0.0;
        if (uFogMode == 0) fogAmount = fogFactorLinear(dist, uFogStart, uFogEnd);
        else if (uFogMode == 1) fogAmount = fogFactorExp(dist, uFogDensity);
        else if (uFogMode == 2) fogAmount = fogFactorExp2(dist, uFogDensity);

        // 4. 원본 픽셀과 포그 색상 블렌딩
        fragColor = vec4(mix(sceneColor, uFogColor, fogAmount), 1.0);
}
