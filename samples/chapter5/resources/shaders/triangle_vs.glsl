#version 410 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

uniform mat4 mvp;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void)
{
        gl_Position = mvp * position;
        vs_out.color = color;
}
