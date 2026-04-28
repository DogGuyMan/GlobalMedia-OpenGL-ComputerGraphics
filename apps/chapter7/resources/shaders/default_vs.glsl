// #version 430 core
#version 410 core

layout(location = 0) in vec4 inVertexPositions;
layout(location = 1) in vec4 inVertexColors;
layout(location = 2) in vec2 inVertexUVs;
layout(location = 3) in vec3 inVertexNormals;

uniform mat4 inModelMat;
uniform mat4 inViewMat;
uniform mat4 inProjMat;

out outVertexData {
        vec4 color;
        vec2 uvCoord;
        vec3 worldPos; // FS 에서 light/view 방향 계산용
        vec3 worldNormal; // (M^-1)^T 로 변환된 월드 법선
} vs_out;

void main(void) {
        vec4 mPos = inModelMat * inVertexPositions;
        vec4 vPos = inViewMat * mPos;
        vec4 pPos = inProjMat * vPos;

        gl_Position = pPos;
        vs_out.color = inVertexColors;
        vs_out.uvCoord = inVertexUVs;
        vs_out.worldPos = mPos.xyz;

        // Normal Matrix = (M^-1)^T : 비균일 스케일에서도 법선을 정확히 변환.
        // 평행이동은 법선에 영향이 없으므로 mat3 만 추출.
        mat3 normalMat = mat3(transpose(inverse(inModelMat)));
        vs_out.worldNormal = normalize(normalMat * inVertexNormals);
}
