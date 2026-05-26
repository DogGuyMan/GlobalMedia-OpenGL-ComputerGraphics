#version 410 core

// ScreenQuadStage passthrough fragment shader — FBO color attachment 그대로 출력.
// uScene 컨벤션 (SP4 migrate_demo 정통).

uniform sampler2D uScene;

in vec2 vUV;
out vec4 fragColor;

void main()
{
        fragColor = texture(uScene, vUV);
}
