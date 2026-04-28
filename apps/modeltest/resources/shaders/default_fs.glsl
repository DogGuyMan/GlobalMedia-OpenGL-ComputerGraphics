// #version 430 core
#version 410 core

out vec4 outBufferColor;

uniform vec4 inBaseColor;

uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;

uniform float uTex1Used;
uniform float uTex2Used;
uniform float uTex3Used;
uniform float uTex4Used;

uniform vec2 inUVOffset1;
uniform vec2 inUVOffset2;
uniform vec2 inUVOffset3;
uniform vec2 inUVOffset4;

uniform vec2 inUVRatio1;
uniform vec2 inUVRatio2;
uniform vec2 inUVRatio3;
uniform vec2 inUVRatio4;

in VS_OUT {
        vec4 color;
        vec2 uvCoord;
} fs_in;

void main(void) {
        vec4 resColor = vec4(1.0, 1.0, 1.0, 1.0);

        if (uTex1Used >= 0.99) {
                vec2 uv = inUVOffset1 + fs_in.uvCoord * inUVRatio1;
                vec4 layer = texture(tex1, uv);
                float opacity = layer.a;
                resColor = layer * opacity + resColor * (1.0 - opacity);
        }
        if (uTex2Used >= 0.99) {
                vec2 uv = inUVOffset2 + fs_in.uvCoord * inUVRatio2;
                vec4 layer = texture(tex2, uv);
                float opacity = layer.a;
                resColor = layer * opacity + resColor * (1.0 - opacity);
        }
        if (uTex3Used >= 0.99) {
                vec2 uv = inUVOffset3 + fs_in.uvCoord * inUVRatio3;
                vec4 layer = texture(tex3, uv);
                float opacity = layer.a;
                resColor = layer * opacity + resColor * (1.0 - opacity);
        }
        if (uTex4Used >= 0.99) {
                vec2 uv = inUVOffset4 + fs_in.uvCoord * inUVRatio4;
                vec4 layer = texture(tex4, uv);
                float opacity = layer.a;
                resColor = layer * opacity + resColor * (1.0 - opacity);
        }

        outBufferColor = inBaseColor * fs_in.color * resColor;
        // outBufferColor = vec4(1.0, 1.0, 1.0, 1.0);
}
