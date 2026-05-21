#version 410 core

// migrate_demo window VS — 투명 텍스쳐 패스. aTexCoord 만 전달.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec2 vsTexCoord;

void main()
{
    vsTexCoord  = aTexCoord;
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
