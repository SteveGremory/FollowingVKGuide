#include <vk_mesh.h>

VertexInputDescription Vertex::get_vertex_input_desc() {
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