#version 410 core

layout(location = 0) out vec4 color;

in VS_OUT {
        vec4 color;
        vec2 vsTexCoord;
} fs_in;

uniform sampler2D tex1;

void main(void)
{
        color = fs_in.color * texture(tex1, fs_in.vsTexCoord);
}
