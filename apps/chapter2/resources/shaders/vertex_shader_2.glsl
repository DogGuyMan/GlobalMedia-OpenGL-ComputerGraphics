#version 410 core

void main(void)
{
	// 기본 삼각형 (바람개비 날개 하나)
	const vec4 base[3] = vec4[3](
		vec4(0.0, 0.0, 0.5, 1.0),
		vec4(-0.5, 0.5, 0.5, 1.0),
		vec4(0.0, 0.5, 0.5, 1.0)
	);

	// 12개 정점 (4개 날개 x 3 정점)
	vec4 vertices[12];

	for (int i = 0; i < 4; i++) {
		// 90도씩 회전 (i * 90도 = i * PI/2)
		float angle = float(i) * radians(90.0);
		float c = cos(angle);
		float s = sin(angle);

		// Z축 기준 2D 회전 행렬 (xy만 회전)
		mat2 rot = mat2(
			c, s,
			-s, c
		);

		for (int j = 0; j < 3; j++) {
			vec2 rotated = rot * base[j].xy;
			vertices[i * 3 + j] = vec4(rotated, base[j].z, base[j].w);
		}
	}

	gl_Position = vertices[gl_VertexID];
}
