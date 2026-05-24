#version 410 core

in vec2 vUv;

uniform sampler2D uAtlas;
uniform vec4 uTint;

out vec4 fragColor;

void main()
{
    vec4 c = texture(uAtlas, vUv);
    if (c.a < 0.01) discard;   // alpha-test (sorting 회피, spec §10.2)
    fragColor = c * uTint;
}
