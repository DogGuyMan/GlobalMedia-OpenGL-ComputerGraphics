#version 410 core

// 투명 텍스쳐 패스 — alpha discard.
// 레퍼런스 OpenGL-With-CMake/resources/shader/texture.fs 와 동일 동작.

in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D tex0;

void main()
{
    vec4 pixel = texture(tex0, vsTexCoord);
    if (pixel.a < 0.01)
        discard;
    fragColor = pixel;
}
