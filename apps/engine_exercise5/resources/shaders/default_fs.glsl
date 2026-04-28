

#version 410 core

out vec4 outBufferColor;

in VS_OUT {
        vec4 color;
        vec2 uv;
} fs_in;

uniform vec4 inBaseColor;

void main(void) {
        outBufferColor = vec4(1.0, 1.0, 1.0, 1.0);
        outBufferColor *= fs_in.color;
}
