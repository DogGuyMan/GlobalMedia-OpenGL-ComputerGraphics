#version 410 core

in vec2 v_uv;

uniform sampler2D u_atlas;
uniform vec4 u_tint;

out vec4 fragColor;

void main()
{
    vec4 c = texture(u_atlas, v_uv);
    if (c.a < 0.01) discard;   // alpha-test (sorting 문제 회피, spec §10.2)
    fragColor = c * u_tint;
}
