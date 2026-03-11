#version 410 core

void main(void)
{
	const vec4 base[3] = vec4[3](
		vec4(0.0, 0.0, 0.5, 1.0),
		vec4(-0.5, 0.5, 0.5, 1.0),
		vec4(0.0, 0.5, 0.5, 1.0)
	);

	int blade = gl_VertexID / 3;   // 날개 번호 (0~3)
	int vert  = gl_VertexID % 3;   // 날개 내 정점 (0~2)

	// CW 회전: 음의 각도
	float angle = float(blade) * radians(-90.0);
	float c = cos(angle);
	float s = sin(angle);
	mat2 rot = mat2(c, s, -s, c);

	vec2 rotated = rot * base[vert].xy;
	gl_Position = vec4(rotated, base[vert].zw);
}
