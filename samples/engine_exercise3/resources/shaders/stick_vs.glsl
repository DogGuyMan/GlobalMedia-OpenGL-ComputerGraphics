#version 410 core

layout(location = 0) in float CurrentTime;

void main(void)
{
        vec4 positions[6] = vec4[6](
                        vec4(0.01, 0.0, 0.5, 1.0),
                        vec4(-0.01, 0.0, 0.5, 1.0),
                        vec4(-0.01, -5.0, 0.5, 1.0),

                        vec4(0.01, 0.0, 0.5, 1.0),
                        vec4(-0.01, -5.0, 0.5, 1.0),
                        vec4(0.01, -5.0, 0.5, 1.0)
                );

        mat4 mRotMat = mat4(1.0);
        mat4 mMovMat = mat4(1.0);
        mMovMat[3][0] = cos(CurrentTime) * 0.5;

        for (int i = 0; i < 6; i++)
                positions[i] = mMovMat * mRotMat * vec4(positions[i].xyz * 0.5, 1.0);
        gl_Position = positions[gl_VertexID];
}
