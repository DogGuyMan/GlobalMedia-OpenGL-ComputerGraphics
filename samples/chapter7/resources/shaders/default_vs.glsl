// #version 430 core
#version 410 core

layout(location = 0) in vec4 inVertexPositions;
layout(location = 1) in vec4 inVertexColors;
layout(location = 2) in vec4 inVertexNormals;
layout(location = 3) in vec2 inVertexUVs;

uniform mat4 inModelMat;
uniform mat4 inViewMat;
uniform mat4 inProjMat;

out outVertexData {
        vec4 color;
        vec2 uvCoord;
        vec3 normal;
} vs_out;

void main(void) {
        vec4 mPos = inModelMat * inVertexPositions;
        vec4 vPos = inViewMat * mPos;
        vec4 pPos = inProjMat * vPos;

        gl_Position = pPos;
        vs_out.color = inVertexColors;
        vs_out.uvCoord = inVertexUVs;

        vs_out.normal = mat3(transpose(inverse(inModelMat))) * inVertexNormals.xyz;
}
