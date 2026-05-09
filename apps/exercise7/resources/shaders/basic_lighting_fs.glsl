// #version 430 core
#version 410 core

in vec3 vsNormal;
in vec3 vsPos;

uniform vec3 lightColor;
uniform vec3 objectColor;

out vec4 fragColor;

void main()
{
        vec3 result = lightColor * objectColor;
        fragColor = vec4(result, 1.0);
}
