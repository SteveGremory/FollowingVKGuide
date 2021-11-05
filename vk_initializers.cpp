#include <vk_initializers.h>

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
