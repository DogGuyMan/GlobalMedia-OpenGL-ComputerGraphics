#version 410 core

// ScreenQuadStage passthrough fragment shader — FBO color attachment 를 그대로 출력.
// SP-UniversalRenderTarget Phase B — uScene 컨벤션 (SP4 migrate_demo 정통).

uniform sampler2D uScene;

in vec2 vUV;
out vec4 fragColor;

void main()
{
    fragColor = texture(uScene, vUV);
}
