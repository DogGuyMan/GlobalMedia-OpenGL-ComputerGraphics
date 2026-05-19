#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 transformMat;

out vec4 vsColor;
out vec2 vsTexCoord;

void main() {
    gl_Position = transformMat * vec4(aPos, 1.0);
    vsColor = vec4(aColor, 1.0);
    vsTexCoord = aTexCoord;
}
