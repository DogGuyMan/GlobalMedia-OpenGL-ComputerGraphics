#version 410 core

// migrate_demo Phong VS — uModel/uView/uProj + world-space position/normal 출력.

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

void main()
{
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vsPosition = worldPos.xyz;
    // 비균등 scale 대응 — transpose(inverse(mat3(uModel))).
    vsNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vsTexCoord = aTexCoord;
    gl_Position = uProj * uView * worldPos;
}
