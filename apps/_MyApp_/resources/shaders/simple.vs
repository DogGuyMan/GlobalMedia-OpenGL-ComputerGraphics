#version 410 core

// migrate_demo simple VS — uModel/uView/uProj 직접 사용.
// 마커 큐브 / 아웃라인 셸 / 라이팅 없는 단색 오브젝트 공통.

layout (location = 0) in vec3 aPos;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

void main()
{
    gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
