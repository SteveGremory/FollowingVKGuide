cmake_minimum_required(VERSION 3.14)
project(VulkanGuide)

## Testing verified commits
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# For all the dependencies
include(deps/deps.cmake)
# For compiling shaders
include(cmake/shaders.cmake)

set(CMAKE_BUILD_TYPE Debug)

# Add source to this project's executable.
set(SOURCES
    source/main.cc
    source/ui/engine_ui.cc
    source/engine/mesh/mesh.cc
    source/engine/vulkan/engine.cc
    source/engine/initializers/initializers.cc
)

add_executable (Main ${SOURCES})
target_include_directories(Main PUBLIC
    source/engine/mesh
    source/engine/vulkan
    source/engine/initializers
    source/engine/common
)

# Use clang and lincxx
set_target_properties(Main PROPERTIES
    CMAKE_CXX_COMPILER "/usr/bin/clang++"
    CMAKE_C_COMPILER "/usr/bin/clang"
    CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi"
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -v"
    CMAKE_CXX_STANDARD 20
    CMAKE_CXX_STANDARD_REQUIRED ON
)

target_include_directories(Main PUBLIC "${CMAKE_SOURCE_DIR}" ${VULKAN_HADERS_INCLUDE_DIRS} ${DEPS_INCLUDE_DIRS})
target_link_libraries(Main vk-bootstrap VulkanMemoryAllocator glm::glm tinyobjloader ImGUI)
target_link_libraries(Main Vulkan::Vulkan SDL2::SDL2)
