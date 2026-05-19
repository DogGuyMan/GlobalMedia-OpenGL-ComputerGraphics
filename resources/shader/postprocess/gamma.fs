#version 330 core

in vec4 vsColor;
in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D frameTexture;
uniform float gamma;

void main() {
    vec4 pixel = texture(frameTexture, vsTexCoord);
    fragColor = vec4(pow(pixel.rgb, vec3(gamma)), 1.0);
}
