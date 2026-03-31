#version 410 core
layout(location = 0) in float currentTime;
// [1] layout(location = 0) in vec4 vertexColor;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void) {
        const vec4 base[3] = vec4[3](
                        vec4(0.0, 0.0, 0.5, 1.0),
                        vec4(0.0, 0.5, 0.5, 1.0),
                        vec4(-0.5, 0.5, 0.5, 1.0)
                );

        float curTimeCos = cos(currentTime) * 0.5 + 0.5f;
        float curTimeSin = sin(currentTime) * 0.5 + 0.5f;
        vec4 vertexTints[3] = vec4[3](
                        vec4(1, 0, 0, 1.0),
                        vec4(0, 1, 0, 1.0),
                        vec4(0, 0, 1, 1.0)
                );
        vertexTints[0] = (vertexTints[0] * 0.5 + curTimeCos * 0.5);
        vertexTints[1] = (vertexTints[1] * 0.5 + curTimeSin * 0.5);
        vertexTints[2] = (vertexTints[2] * 0.5 + curTimeSin * 0.5);

        int bladeID = gl_VertexID / 3;
        int bladeVertID = gl_VertexID % 3;

        float angle = float(bladeID) * radians(90.0) + currentTime;
        float curRotateCos = cos(angle);
        float curRotateSin = sin(angle);
        mat4 rot = mat4(
                        curRotateCos, curRotateSin, 0, 0,
                        -curRotateSin, curRotateCos, 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1
                );
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

        mat4 lookRotate = mat4(
                        rightVec.x, camUpVec.x, dirVec.x, 0.0,
                        rightVec.y, camUpVec.y, dirVec.y, 0.0,
                        rightVec.z, camUpVec.z, dirVec.z, 0.0,
                        0.0, 0.0, 0.0, 1.0
                );
        mat4 lookMove = mat4(
                        1.0, 0.0, 0.0, 0.0,
                        0.0, 1.0, 0.0, 0.0,
                        0.0, 0.0, 1.0, 0.0,
                        -camPos.x, -camPos.y, -camPos.z, 1.0
                );

        mat4 lookAt = lookRotate * lookMove;

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

        vec4 rotated = rot * base[bladeVertID];
        vec4 moved = mov * rotated;
        vec4 viewed = lookAt * moved;
        vec4 projed = proj * viewed;
        gl_Position = projed;

        vs_out.color = vec4(vertexTints[bladeVertID].rgb, 1.0);
}
