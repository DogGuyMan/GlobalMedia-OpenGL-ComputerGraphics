#version 330 core

in vec4 vsColor;
in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D tex;

void main() {
    vec4 pixel = texture(tex, vsTexCoord);
    if (pixel.a < 0.01)
        discard;
    fragColor = pixel;
}
