#version 410 core

layout(location = 0) out vec4 colors;

in VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} fs_in;

uniform vec4 baseColor;
uniform sampler2D tex1;
uniform sampler2D tex2;

void main(void)
{
        vec4 tex1 = texture(tex1, fs_in.vsTexCoord);
        vec4 tex2 = texture(tex2, fs_in.vsTexCoord);
        colors = tex1;
        // if (tex1.x < 0.9 | tex1.y < 0.9 | tex1.z < 0.9) {
        //         colors = tex2;
        // }
        // s
        // colors = vec4(1.0, 1.0, 1.0, 1.0);
}
