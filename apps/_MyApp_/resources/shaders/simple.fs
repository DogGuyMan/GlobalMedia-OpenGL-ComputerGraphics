#version 410 core

// 단색 출력 — baseColor uniform 한 개만 송신. 광원 무관.

uniform vec4 baseColor;

out vec4 fragColor;

void main()
{
    fragColor = baseColor;
}
