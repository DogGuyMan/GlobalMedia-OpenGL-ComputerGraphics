#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션 — Unity _MainTex 식.

void main() {
    vec4 pixel = texture(uScene, vUV);
    fragColor = vec4(1.0 - pixel.rgb, 1.0);
}
