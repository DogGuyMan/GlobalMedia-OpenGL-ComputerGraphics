#version 410 core

// migrate_demo PostFX VS — ScreenQuad 정점을 NDC 그대로 사용.
// Mesh::CreateScreenQuad 가 만든 정점 (aPos: [-1, 1] NDC, aTexCoord: [0, 1]).

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec2 vsTexCoord;

void main()
{
    vsTexCoord  = aTexCoord;
    gl_Position = vec4(aPos, 1.0);
}
