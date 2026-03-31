#version 410 core
layout(location = 0) in float currentTime;

void main(void) {
        const vec4 base[6] = vec4[6](
                        vec4(0.01, 0.0, 0.0, 1.0),
                        vec4(-0.01, 0.0, 0.0, 1.0),
                        vec4(-0.01, -0.5, 0.0, 1.0),
                        vec4(0.01, 0.0, 0.0, 1.0),
                        vec4(-0.01, -0.5, 0.0, 1.0),
                        vec4(-0.01, -0.5, 0.0, 1.0)
                );

        int bladeID = gl_VertexID / 3;
        int bladeVertID = gl_VertexID % 3;

        mat4 mov = mat4(
                        1.0, 0.0, 0.0, 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        cos(currentTime) * 0.5, 0.0, 0.0, 1.0
                );

        vec3 camPos = vec3(0.2, 0.3, 0.8);
        vec3 targetPos = vec3(0.0, 0.0, 0.5);
        vec3 worldUpVec = vec3(0.0, 1.0, 0.0);
        vec3 dirVec = (camPos - targetPos) / length(camPos - targetPos);
        vec3 rightVec = cross(worldUpVec, dirVec);
        vec3 camUpVec = cross(dirVec, rightVec);

        mat4 lookAt = mat4(
                        rightVec.x, camUpVec.x, dirVec.x, 0.0,
                        rightVec.y, camUpVec.y, dirVec.y, 0.0,
                        rightVec.z, camUpVec.z, dirVec.z, 0.0,
                        0.0, 0.0, 0.0, 1.0
                ) * mat4(
                                1.0, 0.0, 0.0, 0.0,
                                0.0, 1.0, 0.0, 0.0,
                                0.0, 0.0, 1.0, 0.0,
                                -camPos.x, -camPos.y, -camPos.z, 1.0
                        );

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

        vec4 moved = mov * base[bladeVertID];
        vec4 viewed = lookAt * moved;
        vec4 projed = proj * viewed;
        gl_Position = projed;
}
