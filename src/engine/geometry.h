#ifndef __ENGINE_GEOMETRY_H__
#define __ENGINE_GEOMETRY_H__

#include "GL/gl3w.h"
#include "engine/constants.h"

namespace Engine::Model
{
	// CCW 정렬된 3정점에서 face normal 계산 (외적 + 정규화)
	vmath::vec3 ComputeFaceNormal(const vmath::vec4 &p0,
	                              const vmath::vec4 &p1,
	                              const vmath::vec4 &p2);

	void PushVertex(std::vector<GLfloat> &vertices,
	                const vmath::vec4 pos,
	                const vmath::vec4 color,
	                const vmath::vec3 normal,
	                const vmath::vec2 uv,
	                const vmath::vec3 &offset);

	void BuildTriangle(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = Constants::GEOMETRY::TRIANGLE_FACE_INDICES);

	void BuildQuad(
	    std::vector<GLfloat> &buffer_data,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = Constants::GEOMETRY::QUAD_FACE_INDICES);

	void BuildCube(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildCone(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildTetrahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildOctahedron(
	    std::vector<GLfloat> &buffer_data,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildDisk(std::vector<GLfloat> &buffer_data,
	               double us, double ue, int uRes,
	               double vs, double ve, int vRes,
	               float radius = 1.0f,
	               const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	               bool back_face = false);

	void BuildCylinder(std::vector<GLfloat> &buffer_data,
	                   double us, double ue, int uRes,
	                   double vs, double ve, int vRes,
	                   float radius = 1.0f,
	                   float height = 1.0f,
	                   const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                   bool back_face = false);

	void BuildHemiSphere(std::vector<GLfloat> &buffer_data,
	                     double us, double ue, int uRes,
	                     double vs, double ve, int vRes,
	                     float radius = 1.0f,
	                     const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                     bool back_face = false);

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
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = Constants::GEOMETRY::TRIANGLE_FACE_INDICES);

	void BuildQuadIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const std::vector<vmath::vec4> &positions,
	    const std::vector<vmath::vec4> &colors,
	    const std::vector<vmath::vec2> &uvs,
	    const std::vector<GLuint> &position_idxs,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    const std::vector<GLuint> &face_idxs = Constants::GEOMETRY::QUAD_FACE_INDICES);

	void BuildCubeIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildConeIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildTetrahedronIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildOctahedronIndexed(
	    std::vector<GLfloat> &vertices,
	    std::vector<GLuint> &indices,
	    const vmath::vec3 &offset = vmath::vec3(-0.5f, -0.5f, -0.5f),
	    bool back_face = false);

	void BuildDiskIndexed(std::vector<GLfloat> &vertices,
	                      std::vector<GLuint> &indices,
	                      double us, double ue, int uRes,
	                      double vs, double ve, int vRes,
	                      float radius = 1.0f,
	                      const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                      bool back_face = false);

	void BuildCylinderIndexed(std::vector<GLfloat> &vertices,
	                          std::vector<GLuint> &indices,
	                          double us, double ue, int uRes,
	                          double vs, double ve, int vRes,
	                          float radius = 1.0f,
	                          float height = 1.0f,
	                          const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                          bool back_face = false);

	void BuildHemiSphereIndexed(std::vector<GLfloat> &vertices,
	                            std::vector<GLuint> &indices,
	                            double us, double ue, int uRes,
	                            double vs, double ve, int vRes,
	                            float radius = 1.0f,
	                            const vmath::vec3 &offset = vmath::vec3(0.0f, 0.0f, 0.0f),
	                            bool back_face = false);
} // namespace Engine::Model

#endif // __ENGINE_GEOMETRY_H__
