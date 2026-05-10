#version 430 core
// #version 410 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;

out vec3 vsPosition;
out vec3 vsColor;
out vec2 vsTexCoord;
out vec3 vsNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
        vsPosition = vec3(model * vec4(pos, 1.0));
        vsColor = color;
        vsNormal = mat3(transpose(inverse(model))) * normal;
        vsTexCoord = texCoord;

        gl_Position = projection * view * vec4(vsPosition, 1.0);
}
