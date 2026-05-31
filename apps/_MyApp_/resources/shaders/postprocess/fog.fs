#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션 — 다른 PostFX 셰이더와 동일.

// Fog 파라미터 — 스크린 공간 근사 (depth attachment 인프라 없이 동작).
// 진짜 depth-based fog 가 필요하면 SceneFB depth attachment + uInverseProjection 인프라 도입 필요.
uniform vec3  uFogColor   = vec3(0.5, 0.6, 0.7);
uniform float uFogDensity = 0.05;
uniform float uFogStart   = 0.0;   // vUV.y >= start 부터 fog 시작 (0=bottom, 1=top)
uniform float uFogEnd     = 1.0;   // vUV.y >= end 부터 fully fogged
uniform int   uFogMode    = 2;     // 0=Linear, 1=Exp, 2=Exp2

// ==========================================
// glsl-fog 레퍼런스 함수 (depth 대신 스크린 Y 좌표로 dist 근사).
// https://github.com/hughsk/glsl-fog
// ==========================================

float fogFactorLinear(const float dist, const float start, const float end) {
    return 1.0 - clamp((end - dist) / (end - start), 0.0, 1.0);
}

float fogFactorExp(const float dist, const float density) {
    return 1.0 - clamp(exp(-density * dist), 0.0, 1.0);
}

float fogFactorExp2(const float dist, const float density) {
    const float LOG2 = -1.442695;
    float d = density * dist;
    return 1.0 - clamp(exp2(d * d * LOG2), 0.0, 1.0);
}

void main()
{
    vec3 sceneColor = texture(uScene, vUV).rgb;

    // 스크린 공간 dist 근사 — vUV.y (1=top) 가 클수록 멀리 (지평선 위쪽).
    // 0~10 범위로 스케일하여 exp 함수의 의미 있는 범위 활용.
    // Invert — top (vUV.y=1) 이 멀게 → 화면 위쪽이 FogColor 에 쌓임.
    float dist = vUV.y * 10.0;

    float fogAmount = 0.0;
    if (uFogMode == 0)      fogAmount = fogFactorLinear(dist, uFogStart * 10.0, uFogEnd * 10.0);
    else if (uFogMode == 1) fogAmount = fogFactorExp(dist, uFogDensity);
    else                    fogAmount = fogFactorExp2(dist, uFogDensity);

    fragColor = vec4(mix(sceneColor, uFogColor, fogAmount), 1.0);
}
