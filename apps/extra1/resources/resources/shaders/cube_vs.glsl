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
        // 반례, 하면 안되는 코드‼️❌
        // mat4를 vec4에 대입하면 position이 무시되어 정점 위치가 반영되지 않는다.
        // vec4 modeled = modelMat;
        vec4 modeled = modelMat * position;
        vec4 viewed = lookatMat * modeled;
        vec4 projed = projMat * viewed;

        gl_Position = projed;

        vs_out.color = color;
}
