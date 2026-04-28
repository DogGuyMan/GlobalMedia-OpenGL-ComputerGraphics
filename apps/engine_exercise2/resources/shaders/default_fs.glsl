

#version 410 core

out vec4 BufferColor;

in VS_OUT {
        vec4 color;
} fs_in;

void main(void)
{
        BufferColor = fs_in.color;
}
