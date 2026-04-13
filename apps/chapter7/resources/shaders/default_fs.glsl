#version 410 core

layout(location = 0) out vec4 colors;

in VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} fs_in;

// Material uniforms
//   baseColor : vertex color 대체, Material이 주입하는 상수 색상
uniform vec4 baseColor;

void main(void)
{
        colors = baseColor * fs_in.vsColor;
}
