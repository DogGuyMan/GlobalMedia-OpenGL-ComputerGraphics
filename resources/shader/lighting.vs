#version 330 core
// 각 정점 별로 설정된 vertex attribute를 입력받는다.
// 레이아웃은 mesh.h `Vertex { position, normal, texCoord }` 구조체와 1:1 — 3 attributes.
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

uniform mat4 transformMat;
uniform mat4 modelTransformMat;

void main() {
    gl_Position = transformMat * vec4(aPos.xyz, 1.0);
    // 변환전 순수 local 좌표계에서의 normal을 inverse-transpose 로 world space normal 변환.
    vsNormal = (transpose(inverse(modelTransformMat)) * vec4(aNormal, 0.0)).xyz;
    vsTexCoord = aTexCoord;
    // diffuse / specular 계산을 위해 world space position 도 함께 전달.
    vsPosition = (modelTransformMat * vec4(aPos, 1.0)).xyz;
}
