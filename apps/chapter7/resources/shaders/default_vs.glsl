#version 410 core

layout(location = 0) in vec4 positions; // in만 사용
layout(location = 1) in vec4 colors;
layout(location = 2) in vec2 uvCoords;

uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;

out VS_OUT {
        vec4 color;
        vec2 vsTexCoord;
} vs_out;

void main(void) {
        vec4 mPos = modelMat * positions;
        vec4 vPos = viewMat * mPos;
        vec4 pPos = projMat * vPos;

        gl_Position = pPos;
        vs_out.color = colors;
        vs_out.vsTexCoord = uvCoords;
}
