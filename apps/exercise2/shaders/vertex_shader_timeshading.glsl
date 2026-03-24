#version 410 core
layout(location = 0) in float currentTime;
// [1] layout(location = 0) in vec4 vertexColor;

out VS_OUT {
        vec4 color;
} vs_out;

void main(void) {
        const vec4 base[3] = vec4[3](
                        vec4(0.0, 0.0, 0.0, 1.0),
                        vec4(0.0, 0.5, 0.0, 1.0),
                        vec4(-0.5, 0.5, 0.0, 1.0)
                );

        // [2]
        // const vec4 bladeColors[4] = vec4[4](
        // 	vec4(1.0, 0.0, 0.0, 1.0),  // blade 0: 빨강
        // 	vec4(0.0, 1.0, 0.0, 1.0),  // blade 1: 초록
        // 	vec4(0.0, 0.0, 1.0, 1.0),  // blade 2: 파랑
        // 	vec4(1.0, 1.0, 0.0, 1.0)   // blade 3: 노랑
        // );

        // [3]
        float curTimeCos = cos(currentTime);
        float curTimeSin = sin(currentTime);
        vec4 vertexTints[3] = vec4[3](vec4(curTimeCos, curTimeSin, curTimeCos, 1.0), vec4(curTimeSin, curTimeCos, curTimeSin, 1.0), vec4(curTimeSin, curTimeCos, curTimeSin, 1.0));

        int bladeID = gl_VertexID / 3;
        int bladeVertID = gl_VertexID % 3;

        float angle = float(bladeID) * radians(90.0);
        float curRotateCos = cos(angle);
        float curRotateSin = sin(angle);
        mat2 rot = mat2(
                        curRotateCos, curRotateSin,
                        -curRotateSin, curRotateCos
                );
        vec2 rotated = rot * base[bladeVertID].xy;
        gl_Position = vec4(rotated, 0.0, 1.0);
        // [1] vs_out.color = vertexColor;
        // [2] vs_out.color = bladeColors[bladeID];
        // [3]
        vs_out.color = vec4(vertexTints[bladeVertID].rgb, 1.0);
}
