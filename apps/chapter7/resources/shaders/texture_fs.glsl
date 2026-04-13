#version 410 core

layout(location = 0) out vec4 colors;

in VS_OUT {
        vec2 vsTexCoord;
} fs_in;

// Material uniforms
//   baseColor : vertex color 대체, Material이 주입하는 상수 색상
//   tex1      : 2D texture sampler (unit 0) — 추후 multi-texture/cube map 확장 가능
uniform vec4 baseColor;
uniform sampler2D tex1;

void main(void)
{
        colors = baseColor * texture(tex1, fs_in.vsTexCoord);
}
