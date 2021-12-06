#include <iostream>
#include <tiny_obj_loader.h>
#include <vector>
#include <vk_mesh.hh>

VertexInputDescription Vertex::get_vertex_input_desc()
{
        VertexInputDescription description;

        VkVertexInputBindingDescription mainBinding {};

        mainBinding.binding = 0; // number of these to generate
        mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        mainBinding.stride = sizeof(Vertex);

        description.bindings.push_back(mainBinding);

        VkVertexInputAttributeDescription
                positionAttrib {},
                colorAttrib {},
                normalsAttrib {};

        positionAttrib.binding = 0;
        positionAttrib.location = 0;
        positionAttrib.offset = offsetof(Vertex, position);
        positionAttrib.format = VK_FORMAT_R32G32B32_SFLOAT;

        colorAttrib.binding = 0;
        colorAttrib.location = 2;
        colorAttrib.offset = offsetof(Vertex, color);
        colorAttrib.format = VK_FORMAT_R32G32B32_SFLOAT;

        normalsAttrib.binding = 0;
        normalsAttrib.location = 1;
        normalsAttrib.offset = offsetof(Vertex, normals);
        normalsAttrib.format = VK_FORMAT_R32G32B32_SFLOAT;

        description.attributes.push_back(positionAttrib);
        description.attributes.push_back(normalsAttrib);
        description.attributes.push_back(colorAttrib);

        return description;
};

bool Mesh::load_from_obj(const char* filename)
{
        // this is gonna contain the vertex arrays
        tinyobj::attrib_t attrib;

        // contians the info for each separate object file
        std::vector<tinyobj::shape_t> shapes;
        // contains info for the materials of each shape,
        // for some reason we won't use it
        std::vector<tinyobj::material_t> materials;

        std::string warn, err;

        // load the object
        tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);

        if (!warn.empty()) {
                std::cout << "WARN: " << warn << std::endl;
        }
        if (!err.empty()) {
                std::cerr << "ERROR: " << err << std::endl;
        }

        // Loop over shapes
        for (size_t s = 0; s < shapes.size(); s++) {
                // Loop over faces(polygon)
                size_t index_offset = 0;
                for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {

                        // hardcode loading to triangles
                        int fv = 3;

                        // Loop over vertices in the face.
                        for (size_t v = 0; v < fv; v++) {
                                // access to vertex
                                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                                // vertex position
                                tinyobj::real_t vx = attrib.vertices[3 * idx.vertex_index + 0];
                                tinyobj::real_t vy = attrib.vertices[3 * idx.vertex_index + 1];
                                tinyobj::real_t vz = attrib.vertices[3 * idx.vertex_index + 2];
                                // vertex normal
                                tinyobj::real_t nx = attrib.normals[3 * idx.normal_index + 0];
                                tinyobj::real_t ny = attrib.normals[3 * idx.normal_index + 1];
                                tinyobj::real_t nz = attrib.normals[3 * idx.normal_index + 2];

                                tinyobj::real_t red = attrib.colors[3 * size_t(idx.vertex_index) + 0];
                                tinyobj::real_t green = attrib.colors[3 * size_t(idx.vertex_index) + 1];
                                tinyobj::real_t blue = attrib.colors[3 * size_t(idx.vertex_index) + 2];

                                // copy it into our vertex
                                Vertex new_vert;
                                new_vert.position.x = vx;
                                new_vert.position.y = vy;
                                new_vert.position.z = vz;

                                new_vert.normals.x = nx;
                                new_vert.normals.y = ny;
                                new_vert.normals.z = nz;

                                // we are setting the vertex color as the vertex normal. This is just for display purposes
                                new_vert.color = new_vert.normals;

                                _vertices.push_back(new_vert);
                        }
                        index_offset += fv;
                }
        }

        return true;
}
