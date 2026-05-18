#version 410 core

// triangles : 추상 패치 타입 = 삼각형 도메인
// equal_spacing : 테셀레이션된 엣지를 균등 분할
// cw : 생성된 삼각형의 와인딩 순서 = 시계방향(Clockwise)
layout (triangles, equal_spacing, cw) in;

void main(void) 
{
	// gl_TessCoord 
	// 	TCS에서 설정한(5)로 쪼개지라 해서 테셀레이션 엔진이 생성한 
	// 	21개 존재하는 삼각형 무게중심좌표 (무게 중심이 아니다!) 
	// 		삼각형 내부의 임의의 점을 (u,v,w)로 표현하는 좌표계
	//	일명 gl_in[0,1,2]가 이루는 "삼각형 내부의 이 비율 위치에 점 찍어" 라고 지시
	// 	정점 수 = (N+1)(N+2)/2 = 21,  삼각형 수 = N² = 25
	// gl_in : 
	// 	실제 쪼개지기 전 원본 Patch를 이루는 제어점(Vertex) 좌표임
	// gl_Position : 아래 계산의 의미:
    	//	"gl_TessCoord가 가리키는 비율 위치에 실제 3D 좌표를 선형 보간"
	gl_Position = (
		gl_TessCoord.x * gl_in[0].gl_Position + 
		gl_TessCoord.y * gl_in[1].gl_Position + 
		gl_TessCoord.z * gl_in[2].gl_Position
	);
}