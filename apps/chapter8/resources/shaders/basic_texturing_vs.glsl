#version 410 core
// #version 430 core

layout(location = 0) in vec3 pos;
// location = 1 (vertex color) 은 이 쉐이더에서 쓰지 않으므로 선언하지 않는다. (VBO 인터리브 레이아웃은 그대로 유지)
layout(location = 2) in vec2 texCoord;

uniform mat4 transform;

out vec2 vsTexCoord;

void main(void)
{
        gl_Position = transform * vec4(pos, 1.0);

        vsTexCoord = texCoord;
}
