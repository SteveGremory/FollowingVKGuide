#pragma once

#include "engine.hh"
#include "types.hh"

namespace vkutil {
bool load_image_from_file(VulkanEngine& engine, const char* file, AllocatedImage& outImage);
}
