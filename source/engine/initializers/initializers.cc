#include "engine.hh"
#include "initializers.hh"

#include <iostream>
#include <vulkan/vulkan_core.h>

VkCommandPoolCreateInfo vkinit::command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
{
        VkCommandPoolCreateInfo commandPoolInfo {};
        commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolInfo.pNext = nullptr;

        commandPoolInfo.queueFamilyIndex = queueFamilyIndex;
        commandPoolInfo.flags = flags;

        return commandPoolInfo;
}
VkCommandBufferAllocateInfo vkinit::command_buffer_allocate_info(VkCommandPool pool, uint32_t count, VkCommandBufferLevel level)
{
        VkCommandBufferAllocateInfo commandAllocateInfo {};
        commandAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandAllocateInfo.pNext = nullptr;

        commandAllocateInfo.commandPool = pool;
        commandAllocateInfo.level = level; // tell it what level this is, reference: https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap6.html#commandbuffers-lifecycle
        commandAllocateInfo.commandBufferCount = count;

        return commandAllocateInfo;
}

VkPipelineShaderStageCreateInfo vkinit::pipeline_shader_stage_create_info(VkShaderStageFlagBits stage, VkShaderModule shaderModule)
{
        VkPipelineShaderStageCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.pNext = nullptr;

        info.module = shaderModule;
        info.stage = stage;
        info.pName = "main";

        return info;
}

VkPipelineVertexInputStateCreateInfo vkinit::vertex_input_stage_create_info()
{
        VkPipelineVertexInputStateCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.pNext = nullptr;

        info.vertexAttributeDescriptionCount = 0;
        info.vertexBindingDescriptionCount = 0;

        return info;
}
VkPipelineInputAssemblyStateCreateInfo vkinit::input_assembly_create_info(VkPrimitiveTopology topology)
{
        VkPipelineInputAssemblyStateCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        info.pNext = nullptr;

        info.topology = topology;
        info.primitiveRestartEnable = VK_FALSE;

        return info;
}
VkPipelineRasterizationStateCreateInfo vkinit::rasterization_state_create_info(VkPolygonMode polygonMode)
{
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

VkPipelineMultisampleStateCreateInfo vkinit::multisampling_state_create_info(VkSampleCountFlagBits sampleCount)
{
        // Copied this as we aren't gonna use multisampling in the tutorial anyways (for now)
        VkPipelineMultisampleStateCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        info.pNext = nullptr;

        info.sampleShadingEnable = VK_FALSE;
        // multisampling defaulted to no multisampling (1 sample per pixel)
        info.rasterizationSamples = sampleCount;
        info.minSampleShading = 1.0f;
        info.pSampleMask = nullptr;
        info.alphaToCoverageEnable = VK_FALSE;
        info.alphaToOneEnable = VK_FALSE;
        return info;
}
VkPipelineColorBlendAttachmentState vkinit::color_blend_attachment_state()
{
        VkPipelineColorBlendAttachmentState colorAttachmentBlend {};
        // tell it to use RGB_A
        colorAttachmentBlend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        // disable color blending
        colorAttachmentBlend.blendEnable = VK_FALSE;
        return colorAttachmentBlend;
}

VkPipelineLayoutCreateInfo vkinit::pipeline_layout_create_info()
{
        VkPipelineLayoutCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.pNext = nullptr;

        // empty defaults
        info.flags = 0;
        info.setLayoutCount = 0;
        info.pSetLayouts = nullptr;
        info.pushConstantRangeCount = 0;
        info.pPushConstantRanges = nullptr;

        return info;
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device, VkRenderPass pass)
{
        // Make viewport from our stored viewport and scissor
        // no multiple windows or scissors
        VkPipelineViewportStateCreateInfo viewportState {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.pNext = nullptr;

        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        viewportState.pScissors = nullptr;
        viewportState.pViewports = nullptr;

        // setup color blending, no transparency
        // it literally says, "no blend bro" but we still do it for some god awful reason
        VkPipelineColorBlendStateCreateInfo colorBlendStateInfo {};
        colorBlendStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendStateInfo.pNext = nullptr;

        colorBlendStateInfo.logicOpEnable = VK_FALSE;
        colorBlendStateInfo.logicOp = VK_LOGIC_OP_COPY;

        colorBlendStateInfo.attachmentCount = 1;
        colorBlendStateInfo.pAttachments = &_colorBlendAttachment;

        _dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

        _dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        _dynamicStateInfo.pDynamicStates = _dynamicStateEnables.data();
        _dynamicStateInfo.dynamicStateCount = _dynamicStateEnables.size();
        _dynamicStateInfo.flags = 0;

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
        pipelineInfo.pDepthStencilState = &_depthStencilInfo;
        pipelineInfo.pDynamicState = &_dynamicStateInfo;

        VkPipeline newPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
                std::cout << "Failed to create graphics pipeline." << std::endl;
                return VK_NULL_HANDLE;
        } else {
                return newPipeline;
        }
}

VkImageCreateInfo vkinit::create_image_info(VkFormat format, VkImageUsageFlags flags, VkExtent3D extent, VkSampleCountFlagBits sampleCount)
{
        VkImageCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.pNext = nullptr;

        info.format = format;
        info.extent = extent;

        info.imageType = VK_IMAGE_TYPE_2D;

        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.usage = flags;
        info.samples = sampleCount;

        info.tiling = VK_IMAGE_TILING_OPTIMAL;

        return info;
}

VkImageViewCreateInfo vkinit::create_image_view_info(VkFormat format, VkImage image, VkImageAspectFlags flags)
{
        VkImageViewCreateInfo info {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.pNext = nullptr;

        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = format;
        info.image = image;

        info.subresourceRange.aspectMask = flags;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.layerCount = 1;
        info.subresourceRange.levelCount = 1;

        return info;
}

VkPipelineDepthStencilStateCreateInfo vkinit::depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp)
{
        VkPipelineDepthStencilStateCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        info.pNext = nullptr;

        info.depthTestEnable = bDepthTest ? VK_TRUE : VK_FALSE;
        info.depthWriteEnable = bDepthWrite ? VK_TRUE : VK_FALSE;
        info.depthCompareOp = bDepthTest ? compareOp : VK_COMPARE_OP_ALWAYS;
        info.depthBoundsTestEnable = VK_FALSE;
        info.minDepthBounds = 0.0f; // Optional
        info.maxDepthBounds = 1.0f; // Optional
        info.stencilTestEnable = VK_FALSE;

        return info;
}

VkDescriptorSetLayoutBinding vkinit::descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding)
{
        VkDescriptorSetLayoutBinding info = {};
        info.binding = binding;
        info.descriptorCount = 1;
        info.descriptorType = type;
        info.pImmutableSamplers = nullptr;
        info.stageFlags = stageFlags;

        return info;
}

VkWriteDescriptorSet vkinit::write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding)
{
        VkWriteDescriptorSet write = {};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = nullptr;

        write.dstBinding = binding;
        write.dstSet = dstSet;
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = bufferInfo;

        return write;
}

VkFenceCreateInfo vkinit::fence_create_info(VkFenceCreateFlags flags)
{
        VkFenceCreateInfo fenceInfo {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.pNext = nullptr;

        // wait on it before using it on a GPU command for the first time
        fenceInfo.flags = flags;
        return fenceInfo;
}

VkCommandBufferBeginInfo vkinit::command_buffer_begin_info(VkCommandBufferUsageFlags usage)
{
        VkCommandBufferBeginInfo cmdBufferInfo {};
        cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        cmdBufferInfo.pNext = nullptr;
        cmdBufferInfo.flags = usage;
        cmdBufferInfo.pInheritanceInfo = nullptr;
        return cmdBufferInfo;
}

VkSubmitInfo vkinit::sumbit_info(VkCommandBuffer* cmd)
{
        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = cmd;
        return submitInfo;
}
