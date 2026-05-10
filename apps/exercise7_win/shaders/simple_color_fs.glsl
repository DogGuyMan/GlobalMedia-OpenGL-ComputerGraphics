#version 430 core
// #version 410 core

in vec3 vsColor;

out vec4 fragColor;

void main()
{
        fragColor = vec4(vsColor, 1.0);
}
