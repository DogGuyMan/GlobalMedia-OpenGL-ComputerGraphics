#version 410 core
// #version 430 core

layout(location = 0) in vec3 pos;
// location = 1 (vertex color) 은 라이팅 쉐이더에서 쓰지 않으므로 선언하지 않는다. (VBO 인터리브 레이아웃은 그대로 유지)
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;

out vec3 vsPosition;
out vec2 vsTexCoord;
out vec3 vsNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
        vsPosition = vec3(model * vec4(pos, 1.0));
        vsNormal = mat3(transpose(inverse(model))) * normal;
        vsTexCoord = texCoord;

        gl_Position = projection * view * vec4(vsPosition, 1.0);
}
