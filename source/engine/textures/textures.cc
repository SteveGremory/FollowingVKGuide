#include "textures.hh"
#include <iostream>
#include <vulkan/vulkan_core.h>

#include "initializers.hh"
#include "types.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool vkutil::load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage)
{
        int textureHeight, textureWidth, textureChannels;
        // Load in the image as RGBA and store the provided in the said variables
        stbi_uc* pixels = stbi_load(file, &textureWidth, &textureHeight, &textureChannels, STBI_rgb_alpha);

        if (!pixels) {
                std::cerr << "Failed to load texture." << std::endl;
                return false;
        }

        void* pixels_ptr;
        // Times 4 in order to have 4 bytes per pixel.
        VkDeviceSize imageSize = textureHeight * textureHeight * 4;
        // Exactly matches the format that the image is being imported in
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;

        // Allocate temp. buffer to upload texture data
        AllocatedBuffer stagingBuffer = engine.create_buffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_COPY);

        // Copy data to buffer
        void* data;
        vmaMapMemory(engine._allocator, stagingBuffer._allocation, &data);

        memcpy(data, pixels_ptr, static_cast<size_t>(imageSize));

        vmaUnmapMemory(engine._allocator, stagingBuffer._allocation);

        // The image isn't needed anymore
        stbi_image_free(pixels);

        // The part where the image is created
        VkExtent3D imageExtent;
        imageExtent.width = textureWidth;
        imageExtent.height = textureHeight;
        imageExtent.depth = 1;

        VkImageCreateInfo dimg_info = vkinit::create_image_info(imageFormat, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, imageExtent, VK_SAMPLE_COUNT_1_BIT);

        AllocatedImage newImage;
        VmaAllocationCreateInfo allocatedImageInfo {};
        allocatedImageInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        vmaCreateImage(engine._allocator, &dimg_info, &allocatedImageInfo, &newImage._image, &newImage._allocation, nullptr);

        engine.immediate_submit([&](VkCommandBuffer cmd) {
                VkImageSubresourceRange range;
                range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

                range.baseArrayLayer = 0;
                range.layerCount = 1;

                range.baseMipLevel = 0;
                range.levelCount = 1;
        });

        return true;
};
