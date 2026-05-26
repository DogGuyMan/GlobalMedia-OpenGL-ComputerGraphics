#version 410 core

// ScreenQuadStage passthrough fragment shader — FBO color attachment 를 그대로 출력.
// SP-UniversalRenderTarget Phase B: migrate_demo 최종 합성 전용.
// uScene 컨벤션 — SP4 PostFX 체인 정통.

uniform sampler2D uScene;

in vec2 vUV;
out vec4 fragColor;

void main()
{
    fragColor = texture(uScene, vUV);
}
