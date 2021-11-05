cmake_minimum_required(VERSION 3.14)
project(VulkanGuide)

# Add source to this project's executable.
add_executable (Main 
  main.cpp
  vk_engine.cpp
  vk_engine.h
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h)


set_property(TARGET Main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:Main>)

find_package(Vulkan REQUIRED)
add_subdirectory(deps/vk-bootstrap)
target_include_directories(Main PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" ${VULKAN_HEADERS_INCLUDE_DIRS})

target_link_libraries(Main Vulkan::Vulkan SDL2 vk-bootstrap::vk-bootstrap )
