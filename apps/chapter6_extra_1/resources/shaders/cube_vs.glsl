#version 410 core
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

uniform mat4 modelMat;
uniform mat4 lookatMat;
uniform mat4 projMat;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void) {
        // CPU에서 전달받은 행렬로 변환
        vec4 modeled = modelMat;
        vec4 viewed = lookatMat * modeled;
        vec4 projed = projMat * viewed;

        gl_Position = projed;

        vs_out.color = color;
}
