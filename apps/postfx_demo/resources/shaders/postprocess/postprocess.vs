#version 410 core

// SP4 post-processing 공용 vertex shader — NDC clip-space 입력 직접 사용.
// Mesh::CreateScreenQuad 가 (-1,-1)~(1,1) NDC 좌표를 직접 제공하므로
// model/view/proj 곱셈 우회. gl_Position 에 vec4(aPos, 1) 그대로.
//
// Vertex attribute layout — SJH::Vertex 구조체와 일관:
//   0: position (vec3) — NDC 좌표
//   1: normal   (vec3) — 사용 안 함 (skip)
//   2: texCoord (vec2) — UV 0..1

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aUV;

out vec2 vUV;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    vUV = aUV;
}
