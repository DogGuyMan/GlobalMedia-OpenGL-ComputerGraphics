#version 330 core

in vec4 vsColor;
in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D frameTexture;

void main() {
    vec4 pixel = texture(frameTexture, vsTexCoord);
    fragColor = vec4(1.0 - pixel.rgb, 1.0);
}
