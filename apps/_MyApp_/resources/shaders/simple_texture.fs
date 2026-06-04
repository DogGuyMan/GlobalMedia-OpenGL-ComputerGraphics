#version 410 core

// 단색 출력 — baseColor uniform 한 개만 송신. 광원 무관.
out vec4 fragColor;

uniform vec4 baseColor;
uniform sampler2D uTex;

in vec3 vsNormal;
in vec3 vsPosition;
in vec2 vsTexCoord;

void main()
{
        fragColor = texture(uTex, vsTexCoord) * baseColor;
}
