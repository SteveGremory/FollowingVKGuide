cmake_minimum_required(VERSION 3.14)

add_library(ImGUI SHARED

    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui_widgets.cpp"
    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui_demo.cpp"

    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui.h"
    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui.cpp"

    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui_draw.cpp"
    "${CMAKE_SOURCE_DIR}/deps/imgui/imgui_tables.cpp"

    "${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_vulkan.cpp"
    "${CMAKE_SOURCE_DIR}/deps/imgui/backends/imgui_impl_sdl2.cpp"
    
)

set_target_properties(ImGUI PROPERTIES
    CMAKE_CXX_STANDARD 20
    CMAKE_CXX_STANDARD_REQUIRED ON
)

target_link_libraries(ImGUI 
    SDL2::SDL2
    Vulkan::Vulkan
)

target_include_directories(ImGUI PRIVATE 
    "${CMAKE_SOURCE_DIR}/deps/imgui"
    "${CMAKE_SOURCE_DIR}/deps/imgui/backends"
)
