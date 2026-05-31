#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션 — 다른 PostFX 셰이더와 동일.

// GrayScale — 1.0 = 원본 색상, 0.0 = 완전 무채색. ImGui "Health" 가 이 값을 구동.
uniform float uGrayscaleAmount = 1.0;

// Vignetting — grayscale 과 독립. 화면 중앙=원본, 테두리=uVignetteColor.
uniform float uVignetteAmount = 0.5;             // 0.0 = 효과 없음, 1.0 = 강한 비네팅.
uniform vec3  uVignetteColor  = vec3(0.0, 0.0, 0.0); // 테두리에 입힐 색.

void main()
{
    vec3 finalColor = texture(uScene, vUV).rgb;

    // 1. GrayScale — Rec.709 휘도(sobel.fs / bloom.fs 와 동일 가중치)로 무채색 생성 후 mix.
    float luminance = dot(finalColor, vec3(0.299, 0.587, 0.114));
    vec3  grayColor = vec3(luminance);
    finalColor = mix(grayColor, finalColor, uGrayscaleAmount);

    // 2. Vignetting — 화면 중앙(0.5,0.5)으로부터 거리 기반 가우시안 계수.
    float dist          = distance(vUV, vec2(0.5));
    float intensity     = uVignetteAmount * 20.0;          // 0~1 → 0~20 강도.
    float vignetteFactor = exp(-dist * dist * intensity);  // 중앙=1, 외곽→0.
    finalColor = mix(uVignetteColor, finalColor, vignetteFactor);

    fragColor = vec4(finalColor, 1.0);
}
