#version 410 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

uniform mat4 translateMat;
uniform mat4 lookatMat;
uniform mat4 projMat;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void) {
        // stick은 회전 없이 이동 + lookAt + projection만 적용
        vec4 moved = translateMat * position;
        vec4 viewed = lookatMat * moved;
        vec4 projed = projMat * viewed;
        gl_Position = projed;

        vs_out.color = color;
}
