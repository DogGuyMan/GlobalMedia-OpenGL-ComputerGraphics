#version 410 core

layout(location = 0) out vec4 frameBuffercolors;

in VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} fs_in;

uniform vec4 baseColor;

void main(void)
{
        vec4 res = fs_in.vsColor;
        res *= baseColor;
        frameBuffercolors = res;
}
