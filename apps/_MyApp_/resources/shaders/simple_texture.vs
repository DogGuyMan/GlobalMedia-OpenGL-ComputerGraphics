#version 410 core

// migrate_demo simple VS — uModel/uView/uProj 직접 사용.
// 마커 큐브 / 아웃라인 셸 / 라이팅 없는 단색 오브젝트 공통.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main()
{
        vec4 worldPos = uModel * vec4(aPos, 1.0);
        vsPosition = worldPos.xyz;
        vsNormal = mat3(transpose(inverse(uModel))) * aNormal;
        vsTexCoord = aTexCoord;
        gl_Position = uProj * uView * worldPos;
}
