#version 410 core

// ScreenQuadStage vertex shader — NDC clip-space 좌표 직접 사용.
// Mesh::CreateScreenQuad 가 (-1,-1)~(1,1) NDC 좌표 제공.
// model/view/proj 곱셈 우회 — gl_Position = vec4(aPos, 1) 그대로.
//
// Vertex attribute layout (SJH::Vertex 구조체 일관):
//   location 0: position (vec3) — NDC 좌표
//   location 1: normal   (vec3) — 사용 안 함
//   location 2: texCoord (vec2) — UV 0..1

layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUV;

out vec2 vUV;

void main()
{
        gl_Position = vec4(aPos, 1.0);
        vUV = aUV;
}
