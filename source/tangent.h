#include <mikktspace/mikktspace.h>

namespace gfx {
	struct Vertex;
	struct TangentCalculatorMesh {
		Vertex* vertices;
		size_t n_triangles;
	};

	struct TangentCalculator {
		TangentCalculator();
		void calculate_tangents(Vertex* vertices, size_t n_triangles);

	private:        
		SMikkTSpaceInterface interface_{};
		SMikkTSpaceContext context{};

		TangentCalculatorMesh mesh;

		static int get_num_faces(const SMikkTSpaceContext* context);
		static int get_num_vertices_of_face(const SMikkTSpaceContext* context, int face);
		static void get_position(const SMikkTSpaceContext* context, float out_pos[], int face, int vert);
		static void get_normal(const SMikkTSpaceContext* context, float out_normal[], int face, int vert);
		static void get_tex_coord(const SMikkTSpaceContext* context, float out_uv[], int face, int vert);
		static void set_tspace_basic(const SMikkTSpaceContext* context, const float tangent[], float sign, int face, int vert);
	};
}