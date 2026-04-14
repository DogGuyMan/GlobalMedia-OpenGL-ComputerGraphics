#version 410 core

layout(location = 0) out vec4 colors;

in VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} fs_in;

// Material uniforms
//   baseColor : vertex color 대체, Material이 주입하는 상수 색상
//   tex1      : 2D texture sampler (unit 0) — 추후 multi-texture/cube map 확장 가능
uniform vec4 baseColor;
uniform sampler2D tex1;
uniform sampler2D tex2;

void main(void)
{
        // vec4 tex1 = texture(tex1, fs_in.vsTexCoord);
        // vec4 tex2 = texture(tex2, fs_in.vsTexCoord);
        // colors = tex1;
        // if (tex1.x < 0.9 | tex1.y < 0.9 | tex1.z < 0.9) {
        //         colors = tex2;
        // }
        colors = fs_in.vsColor;
}
