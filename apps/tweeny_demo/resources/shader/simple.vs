#version 410 core

layout(location = 0) in vec3 aPos;     // Vertex.position (vec3). xy 만 사용.
layout(location = 1) in vec3 aNormal;  // unused — Vertex 레이아웃 호환만
layout(location = 2) in vec2 aTexCoord;// unused

uniform vec2 uOffset; // (tweenX, rowY) NDC
uniform vec2 uScale;  // (halfWidth, halfHeight) NDC

void main()
{
    gl_Position = vec4(aPos.xy * uScale + uOffset, 0.0, 1.0);
}
