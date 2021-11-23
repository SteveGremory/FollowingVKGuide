cmake_minimum_required(VERSION 3.14)
project(VulkanGuide)

# For all the dependencies
include(deps/deps.cmake)
# For compiling shaders
include(cmake/shaders.cmake)

# Add source to this project's executable.
add_executable (Main 
    main.cpp
    vk_engine.cpp
    vk_engine.h
    vk_types.h
    vk_initializers.cpp
    vk_initializers.h
    vk_mesh.h
    vk_mesh.cpp
)

# Use clang and lincxx
set_target_properties(Main PROPERTIES
    CMAKE_CXX_COMPILER "/usr/bin/clang++"
    CMAKE_C_COMPILER "/usr/bin/clang"
    CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi"
    CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -v"
)

target_include_directories(Main PUBLIC "${CMAKE_SOURCE_DIR}" ${VULKAN_HADERS_INCLUDE_DIRS} ${DEPS_INCLUDE_DIRS})
target_link_libraries(Main vk-bootstrap VulkanMemoryAllocator glm::glm tinyobjloader ImGUI)
target_link_libraries(Main Vulkan::Vulkan SDL2::SDL2)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)