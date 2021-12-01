// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.hh>
#include <vulkan/vulkan_core.h>

namespace vkinit {
VkCommandPoolCreateInfo command_pool_create_info(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
VkCommandBufferAllocateInfo command_buffer_allocate_info(VkCommandPool pool, uint32_t count = 1, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
VkPipelineShaderStageCreateInfo pipeline_shader_stage_create_info(VkShaderStageFlagBits flags, VkShaderModule shaderModule);
VkPipelineVertexInputStateCreateInfo vertex_input_stage_create_info();
VkPipelineInputAssemblyStateCreateInfo input_assembly_create_info(VkPrimitiveTopology topology);
VkPipelineRasterizationStateCreateInfo rasterization_state_create_info(VkPolygonMode polygonMode);
VkPipelineMultisampleStateCreateInfo multisampling_state_create_info(VkSampleCountFlagBits sampleCount);
VkPipelineColorBlendAttachmentState color_blend_attachment_state();
VkPipelineLayoutCreateInfo pipeline_layout_create_info();
VkImageCreateInfo create_image_info(VkFormat format, VkImageUsageFlags flags, VkExtent3D extent, VkSampleCountFlagBits sampleCount);
VkImageViewCreateInfo create_image_view_info(VkFormat format, VkImage image, VkImageAspectFlags flags);
VkPipelineDepthStencilStateCreateInfo depth_stencil_create_info(bool bDepthTest, bool bDepthWrite, VkCompareOp compareOp);
VkDescriptorSetLayoutBinding descriptorset_layout_binding(VkDescriptorType type, VkShaderStageFlags stageFlags, uint32_t binding);
VkWriteDescriptorSet write_descriptor_buffer(VkDescriptorType type, VkDescriptorSet dstSet, VkDescriptorBufferInfo* bufferInfo, uint32_t binding);
VkFenceCreateInfo fence_create_info(VkFenceCreateFlags flags);
}
