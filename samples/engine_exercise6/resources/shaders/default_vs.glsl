#version 410 core

layout(location = 0) in vec4 inVertexPositions;
layout(location = 1) in vec4 inVertexColors;
layout(location = 2) in vec2 inVertexUVs;

uniform mat4 inModelMat;
uniform mat4 inViewMat;
uniform mat4 inProjMat;

out VS_OUT {
        vec4 color;
        vec2 uvCoord;
} vs_out;

void main(void)
{
        vec4 resPosition = inVertexPositions;
        vec4 mPosition = inModelMat * resPosition;
        vec4 vPosition = inViewMat * mPosition;
        vec4 pPosition = inProjMat * vPosition;

        gl_Position = pPosition;
        vs_out.color = inVertexColors;

        // UV 변환은 이제 fragment 셰이더가 슬롯별로 처리한다.
        // 여기서는 원본 UV 를 그대로 통과.
        vs_out.uvCoord = inVertexUVs;
}
