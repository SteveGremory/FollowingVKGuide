cmake_minimum_required(VERSION 3.14)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/imgui.cmake)

add_subdirectory(${CMAKE_SOURCE_DIR}/deps/vk-bootstrap)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/tinyobjloader)
add_subdirectory(${CMAKE_SOURCE_DIR}/deps/glm)

set(DEPS_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/deps/vk-bootstrap
    ${CMAKE_SOURCE_DIR}/deps/VulkanMemoryAllocator
    ${CMAKE_SOURCE_DIR}/deps/tinyobjloader
    ${CMAKE_SOURCE_DIR}/deps/glm
    ${CMAKE_SOURCE_DIR}/deps/imgui
)