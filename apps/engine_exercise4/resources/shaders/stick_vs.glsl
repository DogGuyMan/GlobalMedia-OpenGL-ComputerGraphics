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

        for (int i = 0; i < 6; i++)
                positions[i] = proj * lookat * mMovMat * mRotMat * vec4(positions[i].xyz * 0.5, 1.0);
        gl_Position = positions[gl_VertexID];
}
