#version 410 core

// 출력 패치의 제어점(Vertex) 수를 3개로 지정.
// 패치당 TCS가 3번 실행됨
// 	패치는.. 그 삼각형의 정점 3개가 곧 패치 1개다.
// 		GL_TRIANGLES로 해석하면 Primitive(삼각형)가 되고,
//			삼각형 1개 -> 그대로 삼각형 1개
// 		GL_PATCHES로 해석하면 Patch가 된다.
//			패치 1개   -> TCS에서 레벨 5 설정 -> 삼각형 25개로 분할
layout(vertices = 3) out;

void main(void) {
        // 테셀레이션 레벨 설정 (Invocation 0에서만 수행)
        //	내가 담당하는 출력 제어점(Vertex) 번호
        // 	범위: [0, N-1] (N = layout(vertices = N) out 에서 지정한 값)
        // 	현재 코드는 layout(vertices = 3) -> gl_InvocationID = 0, 1, 2
        // 	정점 순서대로 0, 1, 2
        if (gl_InvocationID == 0) // per-patch 데이터는 1회만 쓰면 충분 -> 0번이 대표로 설정
        {
                gl_TessLevelInner[0] = 5.0; // 중심 방향으로 얼마나 쪼갤지
                gl_TessLevelOuter[0] = 5.0; // 정점 A의 맞은편 변 BC
                gl_TessLevelOuter[1] = 5.0; // 정점 B의 맞은편 변 CA
                gl_TessLevelOuter[2] = 5.0; // 정점 C의 맞은편 변 AB
        }

        // 변형 없이 TES(Tessellation Evaluation Shader)로 넘김
        // gl_out[] 배열의 각 gl_Position = 제어점(Vertex) 위치
        gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}

// 1. Vertex Shader (3 verts)
// 2. TCS: 패치 3개 제어점(Vertex) 수신
// 	패치 1개 (입력: 3개 버텍스)
//       	* Inner/Outer 레벨 설정 (InvocationID 0)
//       	* 위치 패변형 없이 넘김 (모든 InvocationID)
// 		Invocation 0  -> gl_out[0] // gl_InvocationID == 0
// 		Invocation 1  -> gl_out[1] // gl_InvocationID == 1
// 		Invocation 2  -> gl_out[2] // gl_InvocationID == 2
// 3. Tessellator (GPU 고정 기능): 각 모서리를 5등분한다는 의미.
// 4. TES: 각 새 버텍스 위치 계산
