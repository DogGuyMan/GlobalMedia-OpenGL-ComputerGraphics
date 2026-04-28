#version 410 core

layout(location = 0) in float CurrentTime;

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

        // vec4 base_colors[3] = vec4[3](
        //                 vec4(1.0, 0.0, 0.0, 1.0),
        //                 vec4(0.0, 1.0, 0.0, 1.0),
        //                 vec4(0.0, 0.0, 1.0, 1.0)
        //         );

        // // gl_VertexID % 3 == 0: 중심(0), 1: 대각 모서리(√2), 2: 축 위 끝점(1)
        // float distWeights[3] = float[3](0.0, sqrt(2.0), 1.0);

        // vec4 colors[3] = vec4[3](
        //                 (base_colors[0] * 0.5) + (cos(CurrentTime) * 0.5 * distWeights[0]),
        //                 (base_colors[1] * 0.5) + (sin(CurrentTime) * 0.5 * distWeights[1]),
        //                 (base_colors[2] * 0.5) + (sin(CurrentTime) * 0.5 * distWeights[2])
        //         );
        float curTimeCos = cos(CurrentTime) * 0.5 + 0.5f;
        float curTimeSin = sin(CurrentTime) * 0.5 + 0.5f;
        vec4 colors[3] = vec4[3](
                        vec4(1, 0, 0, 1.0),
                        vec4(0, 1, 0, 1.0),
                        vec4(0, 0, 1, 1.0)
                );
        colors[0] = (colors[0] * 0.5 + curTimeCos * 0.5);
        colors[1] = (colors[1] * 0.5 + curTimeSin * 0.5);
        colors[2] = (colors[2] * 0.5 + curTimeSin * 0.5);

        mat4 mRotMat = mat4(
                        cos(CurrentTime), sin(CurrentTime), 0.0, 0.0,
                        -sin(CurrentTime), cos(CurrentTime), 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        0.0, 0.0, 0.0, 1.0
                );

        mat4 mMovMat = mat4(1.0);
        mMovMat[3][0] = cos(CurrentTime) * 0.5;

        vec3 eye = vec3(0.2, 0.3, 0.8);
        vec3 target = vec3(0.0, 0.0, 0.0);
        vec3 worldup = vec3(0.0, 1.0, 0.0);

        vec3 dirVec = normalize(eye - target);
        vec3 rightVec = normalize(cross(worldup, dirVec));
        vec3 camUpVec = cross(dirVec, rightVec);

        mat4 camMove = mat4(1.0);
        camMove[0][3] = -eye.x;
        camMove[1][3] = -eye.y;
        camMove[2][3] = -eye.z;
        camMove = transpose(camMove);

        mat4 camView = mat4(
                        rightVec.x, rightVec.y, rightVec.z, 0.0,
                        camUpVec.x, camUpVec.y, camUpVec.z, 0.0,
                        dirVec.x, dirVec.y, dirVec.z, 0.0,
                        0.0, 0.0, 0.0, 1.0
                );
        camView = transpose(camView);

        mat4 lookat = camView * camMove;

        float left = -0.1;
        float right = 0.1;
        float top = 0.07;
        float bottom = -0.07;
        float near = 0.1;
        float far = 10;

        mat4 proj = mat4(
                        (2 * near) / (right - left), 0, 0, 0,
                        0, (2 * near) / (top - bottom), 0, 0,
                        (right + left) / (right - left), (top + bottom) / (top - bottom), (near + far) / (near - far), -1,
                        0, 0, (2 * near * far) / (near - far), 0
                );

        for (int i = 0; i < 12; i++)
                positions[i] = proj * lookat * mMovMat * mRotMat * vec4(positions[i].xyz * 0.5, 1.0);
        gl_Position = positions[gl_VertexID];
        vs_out.color = colors[gl_VertexID % 3];
}
