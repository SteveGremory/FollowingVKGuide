#pragma once

#include "types.hh"
#include <glm/glm.hpp>
#include <vector>

struct VertexInputDescription {
        std::vector<VkVertexInputBindingDescription> bindings;
        std::vector<VkVertexInputAttributeDescription> attributes;

        VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
        glm::vec3 position;
        glm::vec3 normals;
        glm::vec3 color;

        static VertexInputDescription get_vertex_input_desc();
};

struct Mesh {
        std::vector<Vertex> _vertices;
        AllocatedBuffer _vertexBuffer;

        bool load_from_obj(const char* filename);
};
