#version 410 core

in vec2 vUV;

out vec4 fragColor;

uniform sampler2D uScene;   // SP4 컨벤션.
uniform float gamma;

void main() {
    vec4 pixel = texture(uScene, vUV);
    fragColor = vec4(pow(pixel.rgb, vec3(gamma)), 1.0);
}
