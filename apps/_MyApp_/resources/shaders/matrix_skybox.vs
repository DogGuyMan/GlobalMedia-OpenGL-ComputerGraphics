#version 410 core
layout(location = 0) in vec3 aPos;

out vec3 v_TexCoords;

// 엔진 컨벤션 uniform (SceneRenderer 가 매 카메라 패스마다 송신).
uniform mat4 uProj;
uniform mat4 uView;

void main() {
        v_TexCoords = aPos;

        // view 의 이동(translation) 성분 제거 — 회전만 남겨 박스가 항상 카메라를 감싸게 함.
        // (mat3 로 잘라내면 4번째 열/행의 평행이동이 사라짐 -> 무한 원경 스카이박스)
        mat4 viewNoTrans = mat4(mat3(uView));

        // 깊이 버퍼가 항상 1.0(최대치)를 유지하도록 w 요소를 활용하여 z 값을 w와 같게 설정합니다.
        vec4 pos = uProj * viewNoTrans * vec4(aPos, 1.0);
        gl_Position = pos.xyww;
}
