#include "tangent.h"
#include "scene.h"

namespace gfx {
	TangentCalculator::TangentCalculator() {
		interface_.m_getNumFaces = get_num_faces;
		interface_.m_getNumVerticesOfFace = get_num_vertices_of_face;
		interface_.m_getPosition = get_position;
		interface_.m_getNormal = get_normal;
		interface_.m_getTexCoord = get_tex_coord;
		interface_.m_setTSpaceBasic = set_tspace_basic;
		mesh.n_triangles = 0;
		mesh.vertices = nullptr;
		context.m_pInterface = &interface_;
	}

	void TangentCalculator::calculate_tangents(Vertex* vertices, size_t n_triangles) {
		mesh.vertices = vertices;
		mesh.n_triangles = n_triangles;
		context.m_pUserData = &mesh;
		genTangSpaceDefault(&context);
	}

	int TangentCalculator::get_num_faces(const SMikkTSpaceContext* context) {
		TangentCalculatorMesh* mesh = (TangentCalculatorMesh*)context->m_pUserData;
		return (int)mesh->n_triangles;
	}

	int TangentCalculator::get_num_vertices_of_face(const SMikkTSpaceContext* context, int face) {
		return 3; // We only support triangles
	}

	void TangentCalculator::get_position(const SMikkTSpaceContext* context, float out_pos[], int face, int vert) {
		TangentCalculatorMesh* mesh = (TangentCalculatorMesh*)context->m_pUserData;
		int vertex_id = (face * 3) + vert;
		Vertex vertex = mesh->vertices[vertex_id];
		out_pos[0] = vertex.position.x;
		out_pos[1] = vertex.position.y;
		out_pos[2] = vertex.position.z;
	}

	void TangentCalculator::get_normal(const SMikkTSpaceContext* context, float out_normal[], int face, int vert) {
		TangentCalculatorMesh* mesh = (TangentCalculatorMesh*)context->m_pUserData;
		int vertex_id = (face * 3) + vert;
		Vertex vertex = mesh->vertices[vertex_id];
		out_normal[0] = vertex.normal.x;
		out_normal[1] = vertex.normal.y;
		out_normal[2] = vertex.normal.z;
	}

	void TangentCalculator::get_tex_coord(const SMikkTSpaceContext* context, float out_uv[], int face, int vert) {
		TangentCalculatorMesh* mesh = (TangentCalculatorMesh*)context->m_pUserData;
		int vertex_id = (face * 3) + vert;
		Vertex vertex = mesh->vertices[vertex_id];
		out_uv[0] = vertex.texcoord0.x;
		out_uv[1] = vertex.texcoord0.y;
	}

	void TangentCalculator::set_tspace_basic(const SMikkTSpaceContext* context, const float tangent[], float sign, int face, int vert) {
		TangentCalculatorMesh* mesh = (TangentCalculatorMesh*)context->m_pUserData;
		int vertex_id = (face * 3) + vert;
		Vertex& vertex = mesh->vertices[vertex_id];
		vertex.tangent.x = tangent[0];
		vertex.tangent.y = tangent[1];
		vertex.tangent.z = tangent[2];
		vertex.tangent.w = -sign;
	}
}