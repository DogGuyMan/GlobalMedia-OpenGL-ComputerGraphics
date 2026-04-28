
// #version 430 core
#version 410 core

out vec4 BufferColor;

in VS_OUT {
        vec4 color;
} fs_in;

void main(void)
{
        BufferColor = vec4(1.0, 0.0, 0.0, 1.0);
}
