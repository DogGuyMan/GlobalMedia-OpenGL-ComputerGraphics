#version 410 core
// #version 430 core

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 texCoord;
layout(location = 3) in vec3 normal;

out vec3 vsPosition;
out vec2 vsTexCoord;
out vec3 vsNormal;
out vec3 vsColor;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
        vsPosition = vec3(model * vec4(pos, 1.0));
        vsNormal = mat3(transpose(inverse(model))) * normal;
        vsTexCoord = texCoord;
        vsColor = color;

        gl_Position = projection * view * vec4(vsPosition, 1.0);
}
