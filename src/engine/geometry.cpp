#include "engine/geometry.h"

namespace Engine::Model
{
	using namespace Constants::GEOMETRY;

	// === Utility functions ===
	vmath::vec3 ComputeFaceNormal(const vmath::vec4 &p0,
	                              const vmath::vec4 &p1,
	                              const vmath::vec4 &p2)
	{
		vmath::vec3 e1 = vmath::vec3(p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]);
		vmath::vec3 e2 = vmath::vec3(p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]);
		return vmath::normalize(vmath::cross(e1, e2));
	}

	void PushVertex(std::vector<GLfloat> &vertices,
	                const vmath::vec4 pos,
	                const vmath::vec4 color,
	                const vmath::vec3 normal,
	                const vmath::vec2 uv,
	                const vmath::vec3 &offset)
	{
		vertices.push_back(pos[0] + offset[0]);
		vertices.push_back(pos[1] + offset[1]);
		vertices.push_back(pos[2] + offset[2]);
		vertices.push_back(pos[3]);
		vertices.push_back(color[0]);
		vertices.push_back(color[1]);
		vertices.push_back(color[2]);
		vertices.push_back(color[3]);
		vertices.push_back(normal[0]);
		vertices.push_back(normal[1]);
		vertices.push_back(normal[2]);
		vertices.push_back(uv[0]);
		vertices.push_back(uv[1]);
	}

	// === Builders ===

	/* BuildTriangle: face_idxs winding 순서대로 3정점 -> CCW edge 외적으로 face normal 계산.
	   flat-shading : 같은 면의 3정점 모두 동일한 normal 공유. */
	void BuildTriangle(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset,
	    const std::vector<GLuint> &face_idxs)
	{
		// face_idxs winding 순서대로 3정점 -> CCW edge 외적으로 face normal 계산.
		// flat-shading : 같은 면의 3정점 모두 동일한 normal 공유.
		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		for (int i = 0; i < 3; i++)
		{
			GLuint k = face_idxs[i];
			PushVertex(buffer_data,
			           positions[position_idxs[k]],
			           colors[k],
			           faceNormal,
			           uvs[k],
			           offset);
		}
	}

	/* BuildQuad: 평면 quad 가정 — 첫 3정점만으로 면 법선 계산해서 6정점에 동일 적용 (flat shading). */
	void BuildQuad(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset,
	    const std::vector<GLuint> &face_idxs)
	{
		// 평면 quad 가정 — 첫 3정점만으로 면 법선 계산해서 6정점에 동일 적용 (flat shading).
		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		for (int i = 0; i < 6; i++)
		{
			GLuint k = face_idxs[i];
			PushVertex(buffer_data,
			           positions[position_idxs[k]],
			           colors[k],
			           faceNormal,
			           uvs[QUAD_MESH_UVS_FAN[k]],
			           offset);
		}
	}

	/* 정육면체 6 면 모두 buffer_data 에 추가.
	   offset:    기본 -0.5 -> 로컬 0 ~ 1  큐브를 원점 중심 0,0,0 로 정렬
	   back_face: true 면 모든 면 winding 반전 (skybox 처럼 안쪽에서 보이게) */
	void BuildCube(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset,
	    bool back_face)
	{
		const auto &face_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 6; f++)
			BuildQuad(buffer_data, CUBE_BASE_POSITIONS, COLOR_ALL_WHITE_6,
			          QUAD_BASE_MESH_UVS, CUBE_FACE_INDICES[f], offset, face_idxs);
	}

	/* 사각뿔 : 4 옆면(삼각형) + 바닥 쿼드.
	   offset:    평행이동
	   back_face: winding 반전 (안쪽에서 보이게) */
	void BuildCone(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset,
	    bool back_face)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &quad_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
			              TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f], offset, tri_idxs);
		BuildQuad(buffer_data, CONE_BOTTOM_BASE_POSITION, COLOR_ALL_WHITE_6,
		          QUAD_BASE_MESH_UVS, QUAD_MESH_UVS_FAN, offset, quad_idxs);
	}

	/* 정사면체 */
	void BuildTetrahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset,
	    bool back_face)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, TETRA_BASE_POSITION, COLOR_ALL_WHITE_4,
			              TRIANGLE_BASE_MESH_UVS, TETRA_FACE_INDICES[f], offset, tri_idxs);
	}

	/* 정팔면체 : 위·아래 사각뿔을 붙인 모양 (CONE_SIDE 를 y 축 거울상으로 복제).
	   offset:    평행이동
	   back_face: winding 반전 */
	void BuildOctahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset,
	    bool back_face)
	{
		// 기본은 윗절반=FRONT, 아랫절반=BACK (xz 거울상이라 서로 반대 방향).
		// back_face=true 면 둘 다 반대로 뒤집음.
		const auto &top_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &bottom_idxs = back_face ? TRIANGLE_FACE_INDICES : TRIANGLE_FACE_INDICES_BACK;

		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
			              TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f], offset, top_idxs);

		std::vector<vmath::vec4> coneDownSideBasePosition;
		vmath::mat4 xzMirrorMat = vmath::mat4::identity();
		xzMirrorMat[1][1] = -1;
		for (const auto &pos : CONE_SIDE_BASE_POSITION)
			coneDownSideBasePosition.push_back(pos * xzMirrorMat);

		for (int f = 0; f < 4; f++)
			BuildTriangle(buffer_data, coneDownSideBasePosition, COLOR_ALL_WHITE_4,
			              TRIANGLE_BASE_INV_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
			              offset, bottom_idxs);
	}

	/* 원판/고리(ring) 파라메트릭 서피스 : XZ 평면, y = 0.
	   us, ue:  시작/끝 각도 [rad]. (0, 2 * M_PI) 면 완전한 원
	   uRes:    각도 분할 수 : 정점은 uRes+1 개 (끝이 시작과 겹침)
	   vs, ve:  반지름 비율 0 ~ 1. vs=0 -> 꽉 찬 디스크, vs>0 -> 고리(ring)
	   vRes:    반지름 분할 수 (쿼드 스트립 row 개수 = vRes)
	   radius:  최대 반지름 (ve 지점의 실제 크기)
	   offset:  평행이동
	   back_face: winding 반전 */
	void BuildDisk(std::vector<GLfloat> &buffer_data,
	               double us, double ue, int uRes, // 각도 (0 ~ 2*M_PI)
	               double vs, double ve, int vRes, // 반지름 비율 (0 ~ 1)
	               float radius,
	               const vmath::vec3 &offset,
	               bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;

		double deltaRad = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentRad = (vs + row * deltaRad) * radius;
				double currentAngle = (us + col * deltaAngle);

				positions.push_back(vmath::vec4(
				    currentRad * cos(currentAngle),
				    0.0,
				    -currentRad * sin(currentAngle),
				    1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
			}
		}

		// XZ 평면 디스크의 법선 = +Y (CCW front) / -Y (back_face)
		const vmath::vec3 diskNormal = back_face ? vmath::vec3(0.0f, -1.0f, 0.0f)
		                                         : vmath::vec3(0.0f, 1.0f, 0.0f);

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3}; // CCW
				int indices_back[] = {p0, p2, p1, p0, p3, p2};  // CW
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], diskNormal, uvs[indices[i]], offset);
			}
		}
	}

	/* 원기둥 옆면 — XZ 평면에 base, +Y 방향으로 높이 height.
	   us, ue, uRes: 원주 각도 범위 [rad] 와 분할 수 (uRes+1 정점)
	   vs, ve, vRes: 높이 비율 (0 ~ 1) 와 분할 수. 실제 y = vs..ve 가 * height
	   radius:  기둥 반지름
	   height:  기둥 높이
	   offset:  평행이동
	   back_face: winding 반전 (기본 = 바깥에서 보이게) */
	void BuildCylinder(std::vector<GLfloat> &buffer_data,
	                   double us, double ue, int uRes, // 각도 (0 ~ 2*M_PI)
	                   double vs, double ve, int vRes, // 높이 비율 (0 ~ 1)
	                   float radius,
	                   float height,
	                   const vmath::vec3 &offset,
	                   bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;
		std::vector<vmath::vec3> normals;

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = (vs + row * deltaV) * height;
				double currentAngle = (us + col * deltaAngle);

				positions.push_back(vmath::vec4(
				    radius * cos(currentAngle),
				    currentV,
				    -radius * sin(currentAngle),
				    1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				// 옆면 법선은 축에서 바깥으로 — y 성분 0, xz 만 라디알.
				// back_face 면 안쪽으로 뒤집힌 법선 사용.
				vmath::vec3 outN(cos(currentAngle), 0.0f, -sin(currentAngle));
				normals.push_back(back_face ? -outN : outN);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3};
				int indices_back[] = {p0, p2, p1, p0, p3, p2};
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], normals[indices[i]], uvs[indices[i]], offset);
			}
		}
	}

	/* 반구 (북반구, +Y 방향) — 중심 원점의 구 절반.
	   us, ue, uRes: 경도 [rad] 범위와 분할 (uRes+1 정점)
	   vs, ve, vRes: 위도 비율 0 ~ 1 — 내부에서 ( * M_PI/2), 0 = 적도(y=0), 1 = 북극(y=radius)
	   radius:  구 반지름
	   offset:  평행이동
	   back_face: winding 반전 */
	void BuildHemiSphere(std::vector<GLfloat> &buffer_data,
	                     double us, double ue, int uRes, // 경도 (0 ~ 2*M_PI)
	                     double vs, double ve, int vRes, // 위도 비율 (0 ~ 1 -> PI/2)
	                     float radius,
	                     const vmath::vec3 &offset,
	                     bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		std::vector<vmath::vec4> positions;
		std::vector<vmath::vec4> colors;
		std::vector<vmath::vec2> uvs;
		std::vector<vmath::vec3> normals;

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = vs + row * deltaV;
				double latitude = currentV * (M_PI / 2.0);
				double currentAngle = (us + col * deltaAngle);

				double r = radius * cos(latitude);
				double y = radius * sin(latitude);

				float px = (float)(r * cos(currentAngle));
				float py = (float)y;
				float pz = (float)(-r * sin(currentAngle));

				positions.push_back(vmath::vec4(px, py, pz, 1.0f));
				uvs.push_back(vmath::vec2((float)col / uRes, (float)row / vRes));
				colors.push_back(vmath::vec4(1.0f, 1.0f, 1.0f, 1.0f));
				// 중심이 원점인 구면 — 법선은 위치 벡터를 정규화한 값.
				vmath::vec3 outN = vmath::normalize(vmath::vec3(px, py, pz));
				normals.push_back(back_face ? -outN : outN);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				int p0 = row * numCols + col;
				int p1 = row * numCols + (col + 1);
				int p2 = (row + 1) * numCols + (col + 1);
				int p3 = (row + 1) * numCols + col;
				int indices_front[] = {p0, p1, p2, p0, p2, p3};
				int indices_back[] = {p0, p2, p1, p0, p3, p2};
				const int *indices = back_face ? indices_back : indices_front;
				for (int i = 0; i < 6; i++)
					PushVertex(buffer_data, positions[indices[i]], colors[indices[i]], normals[indices[i]], uvs[indices[i]], offset);
			}
		}
	}

	// =================================================
	// === Indexed Builders (VBO + EBO 진짜 인덱싱) ===
	// =================================================

	void BuildTriangleIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset,
	    const std::vector<GLuint> &face_idxs)
	{
		const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		// 삼각형은 3 정점 = 3 unique. 인덱스도 0,1,2 그대로.
		for (int i = 0; i < 3; i++)
		{
			GLuint k = face_idxs[i];
			PushVertex(vertices, positions[position_idxs[k]], colors[k], faceNormal, uvs[k], offset);
		}
		indices.push_back(base + 0);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
	}

	void BuildQuadIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset,
	    const std::vector<GLuint> &face_idxs)
	{
		const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

		const vmath::vec4 &fp0 = positions[position_idxs[face_idxs[0]]];
		const vmath::vec4 &fp1 = positions[position_idxs[face_idxs[1]]];
		const vmath::vec4 &fp2 = positions[position_idxs[face_idxs[2]]];
		const vmath::vec3 faceNormal = ComputeFaceNormal(fp0, fp1, fp2);

		// 펼친 6 정점을 순회하며 (position_idxs[k], QUAD_MESH_UVS_FAN[k]) 쌍을 키로 dedupe.
		// FRONT/BACK 모두 자동 처리 — winding 반전은 face_idxs reorder 가 인덱스 순서를 바꿔서 해결.
		// 전제: face_idxs / position_idxs / QUAD_MESH_UVS_FAN 조합이 평면 quad 의 표준 패턴
		//       (6 → 4 unique). 비표준 입력으로 5+ unique 가 나오면 seenPos[4]/seenUv[4] 가 overrun.
		int localIdx[6];
		GLuint seenPos[4];
		GLuint seenUv[4];
		int uniqueCount = 0;
		for (int i = 0; i < 6; i++)
		{
			GLuint k = face_idxs[i];
			GLuint pkey = position_idxs[k];
			GLuint ukey = QUAD_MESH_UVS_FAN[k];
			int found = -1;
			for (int j = 0; j < uniqueCount; j++)
			{
				if (seenPos[j] == pkey && seenUv[j] == ukey)
				{
					found = j;
					break;
				}
			}
			if (found < 0)
			{
				found = uniqueCount;
				seenPos[uniqueCount] = pkey;
				seenUv[uniqueCount] = ukey;
				uniqueCount++;
				// 첫 등장의 colors[k] 만 저장 — 같은 (pkey, ukey) 쌍이 다른 color 라면 무시.
				// 현 사용처는 모두 COLOR_ALL_WHITE_* 이라 영향 없음.
				PushVertex(vertices, positions[pkey], colors[k], faceNormal, uvs[ukey], offset);
			}
			localIdx[i] = found;
		}
		// uniqueCount 는 항상 4 (평면 quad).
		for (int i = 0; i < 6; i++)
			indices.push_back(base + static_cast<GLuint>(localIdx[i]));
	}

	void BuildCubeIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                      const vmath::vec3 &offset, bool back_face)
	{
		const auto &face_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 6; f++)
			BuildQuadIndexed(vertices, indices,
			                 CUBE_BASE_POSITIONS, COLOR_ALL_WHITE_6,
			                 QUAD_BASE_MESH_UVS, CUBE_FACE_INDICES[f],
			                 offset, face_idxs);
	}

	void BuildConeIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                      const vmath::vec3 &offset, bool back_face)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &quad_idxs = back_face ? QUAD_FACE_INDICES_BACK : QUAD_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangleIndexed(vertices, indices,
			                     CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
			                     TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
			                     offset, tri_idxs);
		BuildQuadIndexed(vertices, indices,
		                 CONE_BOTTOM_BASE_POSITION, COLOR_ALL_WHITE_6,
		                 QUAD_BASE_MESH_UVS, QUAD_MESH_UVS_FAN,
		                 offset, quad_idxs);
	}

	void BuildTetrahedronIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                             const vmath::vec3 &offset, bool back_face)
	{
		const auto &tri_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		for (int f = 0; f < 4; f++)
			BuildTriangleIndexed(vertices, indices,
			                     TETRA_BASE_POSITION, COLOR_ALL_WHITE_4,
			                     TRIANGLE_BASE_MESH_UVS, TETRA_FACE_INDICES[f],
			                     offset, tri_idxs);
	}

	void BuildOctahedronIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                            const vmath::vec3 &offset, bool back_face)
	{
		const auto &top_idxs = back_face ? TRIANGLE_FACE_INDICES_BACK : TRIANGLE_FACE_INDICES;
		const auto &bottom_idxs = back_face ? TRIANGLE_FACE_INDICES : TRIANGLE_FACE_INDICES_BACK;

		for (int f = 0; f < 4; f++)
			BuildTriangleIndexed(vertices, indices,
			                     CONE_SIDE_BASE_POSITION, COLOR_ALL_WHITE_4,
			                     TRIANGLE_BASE_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
			                     offset, top_idxs);

		std::vector<vmath::vec4> coneDownSideBasePosition;
		vmath::mat4 xzMirrorMat = vmath::mat4::identity();
		xzMirrorMat[1][1] = -1;
		for (const auto &pos : CONE_SIDE_BASE_POSITION)
			coneDownSideBasePosition.push_back(pos * xzMirrorMat);

		for (int f = 0; f < 4; f++)
			BuildTriangleIndexed(vertices, indices,
			                     coneDownSideBasePosition, COLOR_ALL_WHITE_4,
			                     TRIANGLE_BASE_INV_MESH_UVS, CONE_SIDE_FACE_INDICES[f],
			                     offset, bottom_idxs);
	}

	void BuildDiskIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                      double us, double ue, int uRes,
	                      double vs, double ve, int vRes,
	                      float radius,
	                      const vmath::vec3 &offset, bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

		double deltaRad = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		const vmath::vec3 diskNormal = back_face ? vmath::vec3(0.0f, -1.0f, 0.0f)
		                                         : vmath::vec3(0.0f, 1.0f, 0.0f);
		const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentRad = (vs + row * deltaRad) * radius;
				double currentAngle = (us + col * deltaAngle);

				vmath::vec4 pos(
				    (float)(currentRad * cos(currentAngle)),
				    0.0f,
				    (float)(-currentRad * sin(currentAngle)),
				    1.0f);
				vmath::vec2 uv((float)col / uRes, (float)row / vRes);
				PushVertex(vertices, pos, white, diskNormal, uv, offset);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
				GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
				GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
				GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
				if (!back_face)
				{
					indices.push_back(p0);
					indices.push_back(p1);
					indices.push_back(p2);
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p3);
				}
				else
				{
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p1);
					indices.push_back(p0);
					indices.push_back(p3);
					indices.push_back(p2);
				}
			}
		}
	}

	void BuildCylinderIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                          double us, double ue, int uRes,
	                          double vs, double ve, int vRes,
	                          float radius, float height,
	                          const vmath::vec3 &offset, bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = (vs + row * deltaV) * height;
				double currentAngle = (us + col * deltaAngle);

				vmath::vec4 pos(
				    (float)(radius * cos(currentAngle)),
				    (float)currentV,
				    (float)(-radius * sin(currentAngle)),
				    1.0f);
				vmath::vec2 uv((float)col / uRes, (float)row / vRes);
				vmath::vec3 outN((float)cos(currentAngle), 0.0f, (float)-sin(currentAngle));
				vmath::vec3 normal = back_face ? -outN : outN;
				PushVertex(vertices, pos, white, normal, uv, offset);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
				GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
				GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
				GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
				if (!back_face)
				{
					indices.push_back(p0);
					indices.push_back(p1);
					indices.push_back(p2);
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p3);
				}
				else
				{
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p1);
					indices.push_back(p0);
					indices.push_back(p3);
					indices.push_back(p2);
				}
			}
		}
	}

	void BuildHemiSphereIndexed(std::vector<GLfloat> &vertices, std::vector<GLuint> &indices,
	                            double us, double ue, int uRes,
	                            double vs, double ve, int vRes,
	                            float radius,
	                            const vmath::vec3 &offset, bool back_face)
	{
		int numCols = uRes + 1;
		int numRows = vRes + 1;

		const GLuint base = static_cast<GLuint>(vertices.size() / VERTEX_LEN);

		double deltaV = (ve - vs) / vRes;
		double deltaAngle = (ue - us) / uRes;

		const vmath::vec4 white(1.0f, 1.0f, 1.0f, 1.0f);

		for (int row = 0; row < numRows; row++)
		{
			for (int col = 0; col < numCols; col++)
			{
				double currentV = vs + row * deltaV;
				double latitude = currentV * (M_PI / 2.0);
				double currentAngle = (us + col * deltaAngle);

				double r = radius * cos(latitude);
				double y = radius * sin(latitude);

				float px = (float)(r * cos(currentAngle));
				float py = (float)y;
				float pz = (float)(-r * sin(currentAngle));

				vmath::vec4 pos(px, py, pz, 1.0f);
				vmath::vec2 uv((float)col / uRes, (float)row / vRes);
				vmath::vec3 outN = vmath::normalize(vmath::vec3(px, py, pz));
				vmath::vec3 normal = back_face ? -outN : outN;
				PushVertex(vertices, pos, white, normal, uv, offset);
			}
		}

		for (int row = 0; row < vRes; row++)
		{
			for (int col = 0; col < uRes; col++)
			{
				GLuint p0 = base + static_cast<GLuint>(row * numCols + col);
				GLuint p1 = base + static_cast<GLuint>(row * numCols + (col + 1));
				GLuint p2 = base + static_cast<GLuint>((row + 1) * numCols + (col + 1));
				GLuint p3 = base + static_cast<GLuint>((row + 1) * numCols + col);
				if (!back_face)
				{
					indices.push_back(p0);
					indices.push_back(p1);
					indices.push_back(p2);
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p3);
				}
				else
				{
					indices.push_back(p0);
					indices.push_back(p2);
					indices.push_back(p1);
					indices.push_back(p0);
					indices.push_back(p3);
					indices.push_back(p2);
				}
			}
		}
	}
} // namespace Engine::Model
