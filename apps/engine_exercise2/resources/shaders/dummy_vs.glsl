// #version 430 core
#version 410 core

out VS_OUT {
        vec4 color;
} vs_out;

void main(void)
{
        vec4 positions[12] = vec4[12](
                        vec4(0.0, 0.0, 0.5, 1.0),
                        vec4(1.0, 1.0, 0.5, 1.0),
                        vec4(0.0, 1.0, 0.5, 1.0),

                        vec4(0.0, 0.0, 0.5, 1.0),
                        vec4(-1.0, 1.0, 0.5, 1.0),
                        vec4(-1.0, 0.0, 0.5, 1.0),

                        vec4(0.0, 0.0, 0.5, 1.0),
                        vec4(-1.0, -1.0, 0.5, 1.0),
                        vec4(0.0, -1.0, 0.5, 1.0),

                        vec4(0.0, 0.0, 0.5, 1.0),
                        vec4(1.0, -1.0, 0.5, 1.0),
                        vec4(1.0, 0.0, 0.5, 1.0)
                );

        for (int i = 0; i < 12; i++)
                positions[i] = vec4(
                                positions[i].xyz * 0.5, positions[i].a
                        );

        gl_Position = positions[gl_VertexID];
        vs_out.color = vec4(1.0, 0.0, 0.0, 1.0);
}
