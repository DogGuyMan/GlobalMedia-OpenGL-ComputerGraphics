#version 410 core
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

uniform mat4 rotMat;
uniform mat4 translateMat;
uniform mat4 lookatMat;
uniform mat4 projMat;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void) {
        // CPU에서 전달받은 행렬로 변환
        vec4 rotated = rotMat * position;
        vec4 moved = translateMat * rotated;
        vec4 viewed = lookatMat * moved;
        vec4 projed = projMat * viewed;

        gl_Position = projed;

        vs_out.color = color;
}
