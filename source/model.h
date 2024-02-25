#pragma once
#include <d3d12.h>

#include "common.h"

namespace gfx {
	struct Mesh
	{
		std::vector<uint8_t> vertex_buffer;
		std::vector<uint8_t> index_buffer;
		int size_position = -1;
		int size_normal = -1;
		int size_tangent = -1;
		int size_colour = -1;
		int size_tex_coords = -1;
		int size_indices = -1;
		int index_attr_type = -1;
	};

	struct ModelNode {
		bool is_parent;
		Mesh* mesh;
		ModelNode* parent;
	};
}
