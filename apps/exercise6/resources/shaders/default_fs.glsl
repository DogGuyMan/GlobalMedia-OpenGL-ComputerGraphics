#version 410 core

layout(location = 0) out vec4 frameBuffercolors;

in VS_OUT {
        vec4 vsColor;
        vec2 vsTexCoord;
} fs_in;

// uniform vec4 baseColor;
uniform sampler2D tex1;
uniform sampler2D tex2;

void main(void)
{
        vec4 tex1 = texture(tex1, fs_in.vsTexCoord);
        // vec4 tex2 = texture(tex2, fs_in.vsTexCoord);
        vec4 res = tex1 * fs_in.vsColor;
        // res *= tex1;
        // res = res * baseColor;
        // float adj = (tex2.x + tex2.y + tex2.z) / 3;
        // res *= (tex1 * adj) + (tex2 * (1 - adj));
        frameBuffercolors = res;
}
