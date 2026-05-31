#version 410 core

// _MyApp_ Transparent (unlit) VS
// SJH Vertex layout : aPos(loc0) / aNormal(loc1) / aTexCoord(loc2)
// Matrix uniforms   : uModel / uView / uProj (engine 컨벤션)
// aNormal 은 layout 정합용으로만 선언 — unlit 이라 라이팅 계산 없음.
// UV 는 여기서 결정 — 타일링(uvScale) 후 U 축을 시간(uTime)에 따라 스크롤.

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal; // 미사용 (VAO layout 일치용)
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

uniform vec2 uvScale; // geometry-space 타일링 배수 (REPEAT wrap 전제)
uniform float uTime; // 누적 시간 (MaterialTime 컴포넌트가 매 프레임 갱신)
uniform float uScrollSpeed; // U 방향 스크롤 속도 (tile/sec) — PoliceTape 가 흐르는 효과

out vec2 vsTexCoord;

void main()
{
        vsTexCoord = aTexCoord * uvScale + vec2(uTime * uScrollSpeed, 0.0);
        gl_Position = uProj * uView * uModel * vec4(aPos, 1.0);
}
