﻿#include <vk_initializers.h>
#include <vk_engine.h>

#include <iostream>

VkCommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo commandPoolInfo {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.pNext = nullptr;
    
    commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
    commandPoolInfo.flags = flags;

    return commandPoolInfo;
}
VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level) {
    VkCommandBufferAllocateInfo commandAllocateInfo {};
    commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocateInfo.pNext = nullptr;

    commandAllocateInfo.commandPool = pool;
    commandAllocateInfo.level = level; // tell it what level this is, reference: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap6.html#commandbuffers-lifecycle
    commandAllocateInfo.commandBufferCount = count;

    return commandAllocateInfo;
}


VkPipelineShaderStageCreateInfo        vkinit::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule) {
    VkPipelineShaderStageCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.pNext = nullptr;

    info.module = shaderModule;
    info.stage = stage;
    info.pName = "main";

    return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_stage_create_info() {
    VkPipelineVertexInputStateCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.vertexAttributeDescriptionCount = 0;
    info.vertexBindingDescriptionCount = 0;

    return info;
}
VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(VkPrimitiveTopology topology) {
    VkPipelineInputAssemblyStateCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.topology = topology;
    info.primitiveRestartEnable = VK_FALSE;

    return info;
}
VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(VkPolygonMode polygonMode) {
    VkPipelineRasterizationStateCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    info.pNext = nullptr;
    // set polygon mode:
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : normal triangle drawing
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST    : points
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST     : line-list
    info.polygonMode = polygonMode;
    info.depthClampEnable = VK_FALSE;
    // discards all primitives before the rasterization stage if enabled which we don't want
    info.rasterizerDiscardEnable = VK_FALSE;
    // culling
    info.cullMode = VK_CULL_MODE_NONE;
    info.lineWidth = 1.0f;
    info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    // no depth bias
    info.depthBiasEnable = VK_FALSE;
    info.depthBiasConstantFactor = 0.0f;
    info.depthBiasClamp = 0.0f;
    info.depthBiasSlopeFactor = 0.0f;

    return info;
}

VkPipelineMultisampleStateCreateInfo   vkinit::multisampling_state_create_info() {
    // Copied this as we aren't gonna use multisampling in the tutorial anyways (for now)
    VkPipelineMultisampleStateCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    info.pNext = nullptr;

    info.sampleShadingEnable = VK_FALSE;
    //multisampling defaulted to no multisampling (1 sample per pixel)
    info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    info.minSampleShading = 1.0f;
    info.pSampleMask = nullptr;
    info.alphaToCoverageEnable = VK_FALSE;
    info.alphaToOneEnable = VK_FALSE;
    return info;
}
VkPipelineColorBlendAttachmentState    vkinit::color_blend_attachment_state() {
    VkPipelineColorBlendAttachmentState colorAttachmentBlend {};
    // tell it to use RGB_A
    colorAttachmentBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    // disable color blending
    colorAttachmentBlend.blendEnable = VK_FALSE;
    return colorAttachmentBlend;
}

VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info() {
    VkPipelineLayoutCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.pNext = nullptr;

    //empty defaults
    info.flags = 0;
    info.setLayoutCount = 0;
    info.pSetLayouts = nullptr;
    info.pushConstantRangeCount = 0;
    info.pPushConstantRanges = nullptr;

    return info;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass) {
    // Make viewport from our stored viewport and scissor
    // no multiple windows or scissors
    VkPipelineViewportStateCreateInfo viewportState {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.pNext = nullptr;
    
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    viewportState.pScissors = &_scissor;
    viewportState.pViewports = &_viewport;

    // setup color blending, no transparency
    // it literally says, "no blend bro" but we still do it for some god awful reason
    VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {};
    colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendStateInfo.pNext = nullptr;

    colorBlendStateInfo.logicOpEnable = VK_FALSE;
    colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;

    colorBlendStateInfo.attachmentCount = 1;
    colorBlendStateInfo.pAttachments = &_colorBlendAttachment;

    // finally, build the actual pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = nullptr;

    pipelineInfo.stageCount = _shaderStages.size();
    pipelineInfo.pStages = _shaderStages.data();
    pipelineInfo.pVertexInputState = &_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &_inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &_rasterizer;
    pipelineInfo.pMultisampleState = &_multisampling;
    pipelineInfo.pColorBlendState = &colorBlendStateInfo;
    pipelineInfo.layout = _pipelineLayout;
    pipelineInfo.renderPass = pass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    VkPipeline newPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline ) != VK_SUCCESS) {
        std::cout << "Failed to create graphics pipeline." << std::endl;
        return VK_NULL_HANDLE;
    } else { return newPipeline; }
}
