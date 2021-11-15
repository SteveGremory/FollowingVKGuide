cmake_minimum_required(VERSION 3.14)
project(VulkanGuide)

# Add source to this project's executable.
add_executable (Main 
  main.cpp
  vk_engine.cpp
  vk_engine.h
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_mesh.h
  vk_mesh.cpp)


set_property(TARGET Main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Main>)

find_package(Vulkan REQUIRED)
add_subdirectory(deps/vk-bootstrap)
add_subdirectory(deps/VulkanMemoryAllocator)
add_subdirectory(deps/tinyobjloader)
add_subdirectory(deps/glm)
add_subdirectory(deps/SDL)

target_include_directories(Main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" ${VULKAN_HADERS_INCLUDE_DIRS})

target_link_libraries(Main vk-bootstrap VulkanMemoryAllocator glm::glm tinyobjloader)
target_link_libraries(Main Vulkan::Vulkan SDL2::SDL2)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/compiled/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
        DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
)