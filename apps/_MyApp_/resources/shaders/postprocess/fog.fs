#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene; // 직전 PostFX 출력 (색).
uniform sampler2D uDepth; // sceneFB depth-stencil 텍스처 — .r = 정규화 depth [0,1].
uniform mat4 uInverseProjection; // inverse(WorldCamera projection) — NDC->view 복원.

// Fog 파라미터 — uFogStart/uFogEnd 는 *view 거리 단위* (스크린 비율 아님).
uniform vec3 uFogColor = vec3(0.5, 0.6, 0.7);
uniform float uFogDensity = 0.05;
uniform float uFogStart = 0.0; // Linear 모드 시작 거리.
uniform float uFogEnd = 50.0; // Linear 모드 fully-fogged 거리.
uniform int uFogMode = 2; // 0=Linear, 1=Exp, 2=Exp2.

// ==========================================
// glsl-fog 레퍼런스 함수 (입력 dist = view-space 유클리디안 거리).
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
        float rawDepth = texture(uDepth, vUV).r;

        // D3 — skybox/배경(far plane, depth≈1.0) 은 fog 제외 (하늘 또렷 유지).
        if (rawDepth >= 0.9999)
        {
                fragColor = vec4(sceneColor, 1.0);
                return;
        }

        // NDC -> view-space 복원. perspective divide 로 view 좌표 확정.
        vec4 ndc = vec4(vUV * 2.0 - 1.0, rawDepth * 2.0 - 1.0, 1.0);
        vec4 viewPos = uInverseProjection * ndc;
        viewPos /= viewPos.w;
        float dist = length(viewPos.xyz); // 카메라-픽셀 유클리디안 거리.

        float fogAmount;
        if (uFogMode == 0) fogAmount = fogFactorLinear(dist, uFogStart, uFogEnd);
        else if (uFogMode == 1) fogAmount = fogFactorExp(dist, uFogDensity);
        else fogAmount = fogFactorExp2(dist, uFogDensity);

        fragColor = vec4(mix(sceneColor, uFogColor, fogAmount), 1.0);
}
