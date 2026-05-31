#version 410 core

// _MyApp_ Phong (texture) VS
// SJH Vertex layout : aPos(loc0) / aNormal(loc1) / aTexCoord(loc2)
// Matrix uniforms   : uModel / uView / uProj  (engine 컨벤션)

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vsPosition;
out vec3 vsNormal;
out vec2 vsTexCoord;

void main()
{
    vec4 worldPos  = uModel * vec4(aPos, 1.0);
    vsPosition     = worldPos.xyz;
    vsNormal       = mat3(transpose(inverse(uModel))) * aNormal;
    vsTexCoord     = aTexCoord;
    gl_Position    = uProj * uView * worldPos;
}
