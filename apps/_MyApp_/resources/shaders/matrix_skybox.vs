#version 410 core
layout(location = 0) in vec3 aPos;

out vec3 v_TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main() {
        v_TexCoords = aPos;

        // 깊이 버퍼가 항상 1.0(최대치)를 유지하도록 w 요소를 활용하여 z 값을 w와 같게 설정합니다.
        vec4 pos = projection * view * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
}
