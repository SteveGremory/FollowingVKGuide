#include "imgui_internal.h"
#include "vk_mesh.hh"
#include <SDL_video.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vulkan/vulkan_core.h>
#define VMA_IMPLEMENTATION

#include "VkBootstrap.h"
#include "vk_engine.hh"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>
#include <cstdlib>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>
#include <iostream>
#include <iterator>
#include <vk_initializers.hh>
#include <vk_types.hh>

Uint64 NOW = SDL_GetPerformanceCounter();
Uint64 LAST = 0;
double deltaTime = 0;
int newtime = 0;

/*
    The descriptor set number 0 will be used for engine-global resources, and
   bound once per frame. The descriptor set number 1 will be used for per-pass
   resources, and bound once per pass. The descriptor set number 2 will be used
   for material resources, and the number 3 will be used for per-object
   resources. This way, the inner render loops will only be binding descriptor
   sets 2 and 3, and performance will be high.
*/

#define VK_CHECK(x)                                                                 \
        do {                                                                        \
                VkResult err = x;                                                   \
                if (err) {                                                          \
                        std::cout << "Detected Vulkan error: " << err << std::endl; \
                        abort();                                                    \
                }                                                                   \
        } while (0)

//  Init (Main): SDL and all the vulkan components
void VulkanEngine::init()
{
        // We initialize SDL and create a window with it.
        SDL_Init(SDL_INIT_VIDEO);

        SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

        _window = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED,
                SDL_WINDOWPOS_UNDEFINED, _windowExtent.width,
                _windowExtent.height, window_flags);

        init_vulkan();
        init_swapchain(false);
        init_commands();
        init_default_renderpass();
        init_framebuffers();
        init_sync_structures();
        init_descriptors();
        init_pipelines();
        load_meshes();
        init_scene();
        init_imgui();

        // everything went fine
        _isInitialized = true;
}

void VulkanEngine::init_imgui()
{
        // 1: create descriptor pool for IMGUI
        // the size of the pool is very oversize, but it's copied from imgui demo
        // itself.
        VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        VkDescriptorPool imguiPool;
        VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

        // 2: initialize imgui library

        // this initializes the core structures of imgui
        ImGui::CreateContext();

        // this initializes imgui for SDL
        ImGui_ImplSDL2_InitForVulkan(_window);

        // this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = _instance;
        init_info.PhysicalDevice = _chosen_GPU;
        init_info.Device = _device;
        init_info.Queue = _graphicsQueue;
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = 3;
        init_info.ImageCount = 3;
        init_info.MSAASamples = _sampleCount;

        ImGuiIO& io = ImGui::GetIO();
        ImFont* jetbrainsMono = io.Fonts->AddFontFromFileTTF("../fonts/jetbrains_mono.ttf", 18);
        io.Fonts->Build();
        ImGui::SetCurrentFont(jetbrainsMono);

        ImGui_ImplVulkan_Init(&init_info, _renderpass);

        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.19f, 0.19f, 0.19f, 0.92f);
        colors[ImGuiCol_Border] = ImVec4(0.19f, 0.19f, 0.19f, 0.29f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.34f, 0.34f, 0.34f, 0.54f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.54f);
        colors[ImGuiCol_Button] = ImVec4(0.05f, 0.05f, 0.05f, 0.54f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.19f, 0.19f, 0.19f, 0.54f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.00f, 0.00f, 0.36f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.22f, 0.23f, 0.33f);
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.29f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.40f, 0.44f, 0.47f, 1.00f);
        colors[ImGuiCol_Tab] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 0.36f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.00f, 0.00f, 0.00f, 0.52f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.20f, 0.22f, 0.23f, 1.00f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_NavHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 0.00f, 0.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.00f, 0.00f, 0.35f);

        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(8.00f, 8.00f);
        style.FramePadding = ImVec2(5.00f, 2.00f);
        style.CellPadding = ImVec2(6.00f, 6.00f);
        style.ItemSpacing = ImVec2(6.00f, 6.00f);
        style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing = 25;
        style.ScrollbarSize = 15;
        style.GrabMinSize = 10;
        style.WindowBorderSize = 1;
        style.ChildBorderSize = 1;
        style.PopupBorderSize = 1;
        style.FrameBorderSize = 1;
        style.TabBorderSize = 1;
        style.WindowRounding = 7;
        style.ChildRounding = 4;
        style.FrameRounding = 3;
        style.PopupRounding = 4;
        style.ScrollbarRounding = 9;
        style.GrabRounding = 3;
        style.LogSliderDeadzone = 4;
        style.TabRounding = 4;

        // execute a gpu command to upload imgui font textures
        immediate_submit(
                [&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

        // clear font textures from cpu data
        ImGui_ImplVulkan_DestroyFontUploadObjects();

        // add the destroy the imgui created structures
        _mainDeletionQueue.push_function([=]() {
                vkDestroyDescriptorPool(_device, imguiPool, nullptr);
                ImGui_ImplVulkan_Shutdown();
        });
}

//  Draw: Called every frame, drawcall
void VulkanEngine::draw()
{
        ImGui::Render();
        // that changes now.
        VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._fence, true,
                1000000000));
        VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._fence));

        // request image from the swapchain, one second timeout
        uint32_t swapchainImageIndex;

        auto result = vkAcquireNextImageKHR(_device, _swapchain, 1000000000,
                get_current_frame()._presentSemaphore,
                nullptr, &swapchainImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
                recreate_swapchain();
        }

        VK_CHECK(vkResetCommandBuffer(get_current_frame()._mainCommandBuffer, 0));

        VkCommandBuffer cmd = get_current_frame()._mainCommandBuffer;
        // LET THE COMMAND BUFFER RECORDING BEGIN!
        VkCommandBufferBeginInfo beginInfo {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.pNext = nullptr;

        beginInfo.pInheritanceInfo = nullptr;
        // let vulkan know that we will record this command buffer only once
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        // make a clear-color from frame number. This will flash with a 120*pi frame
        // period.
        VkClearValue clearValue;
        float flash = abs(sin(_frameNumber / 120.f));
        clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

        VkClearValue depthClear;
        depthClear.depthStencil.depth = 1.f;

        // start the main renderpass.
        // We will use the clear color from above, and the framebuffer of the index
        // the swapchain gave us
        VkRenderPassBeginInfo rpInfo = {};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.pNext = nullptr;

        rpInfo.renderPass = _renderpass;
        rpInfo.renderArea.offset.x = 0;
        rpInfo.renderArea.offset.y = 0;
        rpInfo.renderArea.extent = _windowExtent;
        rpInfo.framebuffer = _frameBuffers[swapchainImageIndex];

        // connect clear values
        rpInfo.clearValueCount = 3;
        VkClearValue clearValues[] = { clearValue, clearValue, depthClear };
        rpInfo.pClearValues = &clearValues[0];

        // begin the render pass
        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(_windowExtent.width);
        viewport.height = static_cast<float>(_windowExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor { { 0, 0 }, _windowExtent };
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        // i'll just keep this for the keks, kekw:
        // record???? wtf??? where??? yes now i know, because we didn't have the
        // pipeline back then now. we. do.

        draw_objects(cmd, _renderables.data(), _renderables.size());

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

        // finalize and end the render pass
        vkCmdEndRenderPass(cmd);

        // finalize the command buffer
        VK_CHECK(vkEndCommandBuffer(cmd));

        // prepare for your ass to get shipped
        // wait for the semaphore to say, "alright mate come in, Dr. swapchain is
        // ready to check yo ass" wait for the second semaphore to say, "alright mate,
        // the wait is over; Dr. rendering has finished the surgery"
        VkSubmitInfo submitInfo {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = nullptr;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submitInfo.pWaitDstStageMask = &waitStage;

        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &get_current_frame()._presentSemaphore;

        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &get_current_frame()._renderSemaphore;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        // submit the queue and execute it
        // renderfence will now block everything until this shit finishes shitting
        VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo,
                get_current_frame()._fence));

        // this will put the image we just rendered into the visible window.
        // we want to wait on the _renderSemaphore for that,
        // as it's necessary that drawing commands have finished before the image is
        // displayed to the user
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;

        presentInfo.pSwapchains = &_swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
        presentInfo.waitSemaphoreCount = 1;

        presentInfo.pImageIndices = &swapchainImageIndex;

        auto result_present = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
        if (result_present == VK_ERROR_OUT_OF_DATE_KHR || result_present == VK_SUBOPTIMAL_KHR) {
                _wasResized = true;
                return;
        }

        // increase the number of frames drawn
        _frameNumber++;
}

//  Run: SDL stuff
void VulkanEngine::run()
{
        SDL_Event e;
        bool bQuit = false;
        // main loop
        while (!bQuit) {
                LAST = NOW;
                NOW = SDL_GetPerformanceCounter();

                deltaTime = (double)((NOW - LAST) * 10 / (double)SDL_GetPerformanceFrequency());

                // Handle events on queue
                const Uint8* keyboard_state_array = SDL_GetKeyboardState(NULL);
                _fps = (1.0f / deltaTime) * 10;

                // Left-Right
                if (keyboard_state_array[SDL_SCANCODE_A]) {
                        _camera_positions.x += deltaTime;
                }
                if (keyboard_state_array[SDL_SCANCODE_D]) {
                        _camera_positions.x -= deltaTime;
                }

                // Front-Back
                if (keyboard_state_array[SDL_SCANCODE_W]) {
                        _camera_positions.z += deltaTime;
                }
                if (keyboard_state_array[SDL_SCANCODE_S]) {
                        _camera_positions.z -= deltaTime;
                }

                // Up-Down
                if (keyboard_state_array[SDL_SCANCODE_Q]) {
                        _camera_positions.y += deltaTime;
                }
                if (keyboard_state_array[SDL_SCANCODE_E]) {
                        _camera_positions.y -= deltaTime;
                }

                if (keyboard_state_array[SDL_SCANCODE_W] && keyboard_state_array[SDL_SCANCODE_A]) {
                        _camera_positions.x += deltaTime;
                        _camera_positions.z += deltaTime;
                }

                if (keyboard_state_array[SDL_SCANCODE_W] && keyboard_state_array[SDL_SCANCODE_D]) {
                        _camera_positions.x -= deltaTime;
                        _camera_positions.z += deltaTime;
                }

                if (keyboard_state_array[SDL_SCANCODE_S] && keyboard_state_array[SDL_SCANCODE_A]) {
                        _camera_positions.x += deltaTime;
                        _camera_positions.z -= deltaTime;
                }

                if (keyboard_state_array[SDL_SCANCODE_S] && keyboard_state_array[SDL_SCANCODE_D]) {
                        _camera_positions.x -= deltaTime;
                        _camera_positions.z -= deltaTime;
                }

                // Rotate right-left
                if (keyboard_state_array[SDL_SCANCODE_X]) {
                        _rotation += deltaTime;
                }
                if (keyboard_state_array[SDL_SCANCODE_Z]) {
                        _rotation -= deltaTime;
                }

                while (SDL_PollEvent(&e) != 0) {
                        ImGui_ImplSDL2_ProcessEvent(&e);
                        // close the window when user alt-f4s or clicks the X button
                        if (e.type == SDL_QUIT) {
                                bQuit = true;
                        }
                        if (e.type == SDL_WINDOWEVENT) {
                                if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && e.window.data1 > 0 && e.window.data2 > 0) {

                                        if (_windowExtent.height != e.window.data2 || _windowExtent.width != e.window.data1) {
                                                _wasResized = true;
                                                _windowExtent.height = e.window.data2;
                                                _windowExtent.width = e.window.data1;
                                                recreate_swapchain();
                                        }
                                }
                        }
                }
                // imgui new frame
                ImGui_ImplVulkan_NewFrame();
                ImGui_ImplSDL2_NewFrame(_window);

                ImGui::NewFrame();

                // imgui commands
                ImGui::Begin("Engine Status");
                ImGui::Text("FPS: %d", static_cast<int>(floor(_fps)));
                ImGui::Text("Number Of Meshes: %lu", _meshes.size());
                ImGui::Text("Current Draw Calls: %d", _currentDrawCalls);
                _currentDrawCalls = 0;
                ImGui::End();

                draw();
        }
}

//  Init (Vulkan): Init everything vulkan needs,
//    GPU drivers and all
void VulkanEngine::init_vulkan()
{
        vkb::InstanceBuilder builder;
        auto inst_ret = builder.set_app_name("")
                                .request_validation_layers(true)
                                .require_api_version(1, 1, 0)
                                .use_default_debug_messenger()
                                .build();

        vkb::Instance vkb_inst = inst_ret.value();

        // storing the instance
        _instance = vkb_inst.instance;
        // store the debug messenger
        _debug_messenger = vkb_inst.debug_messenger;

        // Create a surface to render on
        SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

        // use VkBootstrap to detect the presence of neo
        vkb::PhysicalDeviceSelector selector { vkb_inst };
        vkb::PhysicalDevice physicalDevice = selector.set_minimum_version(1, 1)
                                                     .prefer_gpu_device_type(vkb::PreferredDeviceType::integrated)
                                                     .set_surface(_surface)
                                                     .select()
                                                     .value();

        // Check if it's the real neo
        vkb::DeviceBuilder deviceBuilder { physicalDevice };
        vkb::Device vkbDevice = deviceBuilder.build().value();

        // finally.
        _device = vkbDevice.device;
        _chosen_GPU = physicalDevice.physical_device;

        // get the graphics queue from VkBootstrap
        _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
        _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

        // init vulkanmemoryallocator
        VmaAllocatorCreateInfo allocatorInfo {};
        allocatorInfo.device = _device;
        allocatorInfo.physicalDevice = _chosen_GPU;
        allocatorInfo.instance = _instance;
        vmaCreateAllocator(&allocatorInfo, &_allocator);

        // Get the minimum buffer alignment
        vkGetPhysicalDeviceProperties(_chosen_GPU, &_deviceProperties);
        std::cout << "The GPU has a minimum buffer alignment of: "
                  << _deviceProperties.limits.minUniformBufferOffsetAlignment
                  << std::endl;

        _sampleCount = get_max_usable_sample_count();
        _mainDeletionQueue.push_function([&]() { vmaDestroyAllocator(_allocator); });
}

VkSampleCountFlagBits VulkanEngine::get_max_usable_sample_count()
{
        VkSampleCountFlags counts = _deviceProperties.limits.framebufferColorSampleCounts & _deviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_64_BIT" << std::endl;
                return VK_SAMPLE_COUNT_64_BIT;
        } else if (counts & VK_SAMPLE_COUNT_32_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_32_BIT" << std::endl;
                return VK_SAMPLE_COUNT_32_BIT;
        } else if (counts & VK_SAMPLE_COUNT_16_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_16_BIT" << std::endl;
                return VK_SAMPLE_COUNT_16_BIT;
        } else if (counts & VK_SAMPLE_COUNT_8_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_8_BIT" << std::endl;
                return VK_SAMPLE_COUNT_8_BIT;
        } else if (counts & VK_SAMPLE_COUNT_4_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_4_BIT" << std::endl;
                return VK_SAMPLE_COUNT_4_BIT;
        } else if (counts & VK_SAMPLE_COUNT_2_BIT) {
                std::cout << "Max Sample Count Available: "
                          << "VK_SAMPLE_COUNT_2_BIT" << std::endl;
                return VK_SAMPLE_COUNT_2_BIT;
        }
        std::cout << "Max Sample Count Available: VK_SAMPLE_COUNT_1_BIT" << std::endl;
        return VK_SAMPLE_COUNT_1_BIT;
}

//  Init (Swapchain): Init the swapchain and the memory allocator
void VulkanEngine::init_swapchain(bool setOld)
{
        vkb::SwapchainBuilder swapchainBuilder { _chosen_GPU, _device, _surface };

        swapchainBuilder.use_default_format_selection().set_desired_present_mode(
                VK_PRESENT_MODE_FIFO_KHR);

        if (setOld) {
                swapchainBuilder.set_old_swapchain(_swapchain);
        }

        _vkbSwapchain = swapchainBuilder
                                .set_desired_extent(_windowExtent.width, _windowExtent.height)
                                .build()
                                .value();

        // store the swapchain and it's related images
        _swapchain = _vkbSwapchain.swapchain;
        _swapchain_images = _vkbSwapchain.get_images().value();
        _swapchain_image_views = _vkbSwapchain.get_image_views().value();

        _swapchain_image_format = _vkbSwapchain.image_format;

        _swapchainDeletionQueue.push_function(
                [=]() { _oldSwapChain = std::move(_swapchain); });

        // make the size match the window, because obviously
        // we don't want a depth buffer smaller/larger than the viewport
        // because that's fucking stupid (at least afaik)
        VkExtent3D depthImageExtent = { _windowExtent.width, _windowExtent.height, 1 };

        VkExtent3D resolveImageExtent = { _windowExtent.width, _windowExtent.height,
                1 };

        // hardcoding the format to 32 bit float
        _depthImageFormat = VK_FORMAT_D32_SFLOAT;

        // same depth format usage flag
        VkImageCreateInfo dimg_info = vkinit::create_image_info(
                _depthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                depthImageExtent, _sampleCount);
        // for the depth image, we want to alloc it from the GPU's memory
        VmaAllocationCreateInfo depth_buffer_allocation_info {};
        depth_buffer_allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        depth_buffer_allocation_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // alloc and create the image
        vmaCreateImage(_allocator, &dimg_info, &depth_buffer_allocation_info,
                &_depthImage._image, &_depthImage._allocation, nullptr);
        // build an image view for the depth buffer to use for rendering
        VkImageViewCreateInfo dview_info = vkinit::create_image_view_info(
                _depthImageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);
        VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

        // same depth format usage flag
        VkImageCreateInfo resolve_info = vkinit::create_image_info(_swapchain_image_format,
                VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                resolveImageExtent, _sampleCount);
        // for the depth image, we want to alloc it from the GPU's memory
        VmaAllocationCreateInfo resolve_allocation_info {};
        resolve_allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        resolve_allocation_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        // alloc and create the image
        vmaCreateImage(_allocator, &resolve_info, &resolve_allocation_info,
                &_resolveImage._image, &_resolveImage._allocation, nullptr);
        // build an image view for the depth buffer to use for rendering
        VkImageViewCreateInfo resolve_view_info = vkinit::create_image_view_info(
                _swapchain_image_format, _resolveImage._image, VK_IMAGE_ASPECT_COLOR_BIT);
        VK_CHECK(vkCreateImageView(_device, &resolve_view_info, nullptr,
                &_resolveImageView));

        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(_chosen_GPU, &physicalDeviceProperties);

        _swapchainDeletionQueue.push_function([=]() {
                vkDestroyImageView(_device, _depthImageView, nullptr);
                vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);

                vkDestroyImageView(_device, _resolveImageView, nullptr);
                vmaDestroyImage(_allocator, _resolveImage._image,
                        _resolveImage._allocation);
        });
}

//  Cleanup: Run the deletion queue and general cleanup
void VulkanEngine::cleanup()
{
        // pretty self explainatory
        if (_isInitialized) {
                // wait till the GPU is done doing it's thing
                vkDeviceWaitIdle(_device);

                _swapchainDeletionQueue.flush();
                vkDestroySwapchainKHR(_device, _swapchain, nullptr);
                _mainDeletionQueue.flush();

                vkDestroySurfaceKHR(_instance, _surface, nullptr);

                vkDestroyDevice(_device, nullptr);
                vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
                vkDestroyInstance(_instance, nullptr);

                SDL_DestroyWindow(_window);
        }
}

void VulkanEngine::recreate_swapchain()
{
        vkDeviceWaitIdle(_device);
        if (_swapchainDeletionQueue.flush()) {

                init_swapchain(true);
                vkDestroySwapchainKHR(_device, _oldSwapChain, nullptr);

                init_default_renderpass();
                init_framebuffers();

                _wasResized = false;
        }
}

//  Init (Command Buffers): Init the command buffers
void VulkanEngine::init_commands()
{
        /*
        So here's how command buffers work:
        -------------------------------------------------------------
        VkCommandPool
            |
        VkCommandBuffers <- {vkCmdDraw()...other stuff}
            |
        VkQueue
        -------------------------------------------------------------
        -> you can have multiple command buffers in one pool but they
            will be executed in sync, aka, one after the other

        -> you can create multiple command pools on other threads

        -> Once a command buffer has been submitted, it’s still “alive”,
            and being consumed by the GPU, at this point it is NOT safe to reset the
        command buffer yet. You need to make sure that the GPU has finished executing
            all of the commands from that command buffer until you can reset and reuse
        it.

        // btw pools are created and command buffers are allocated to the pools.
      */
        // step 1: create a command pool with the following specifications
        // no need to put this in the for loop
        VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(
                _graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
                VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr,
                        &_frames[i]._commandPool));
                VK_CHECK(vkCreateCommandPool(_device, &uploadCommandPoolInfo, nullptr,
                        &_uploadContext._commandPool));

                // step 2: create commandBuffers
                VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);
                VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo,
                        &_frames[i]._mainCommandBuffer));

                _mainDeletionQueue.push_function([=]() {
                        vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
                        vkDestroyCommandPool(_device, _uploadContext._commandPool, nullptr);
                });
        }
}

//   Init (Render Pass): Init the renderpass
//      all it's attachments and everything
void VulkanEngine::init_default_renderpass()
{

        // the renderpass will use the following color attachment
        VkAttachmentDescription color_attachment {};
        // make the images the same format as the swapchain
        color_attachment.format = _swapchain_image_format;
        // single sample, no MSAA 4u, monke.
        color_attachment.samples = _sampleCount;
        // clear the renderpass if the attachment when loaded
        color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        // keep the attachmant stored when da renderpass be goin home
        color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // we don't care about the stencil
        color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // we don't know or care about the starting layout of the image attachment
        color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // we use the KHR layout (Image is on a layout that allows displaying the
        // image on the screen)
        color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        // Now that our main image target is defined, we need to add a subpass that
        // will render into it.
        VkAttachmentReference color_attachment_ref {};
        color_attachment_ref.attachment = 0;
        color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Image is on a layout optimal
                                                                                // to be written into by
                                                                                // rendering commands.

        VkAttachmentDescription depth_attachment {};
        depth_attachment.flags = 0;
        depth_attachment.format = _depthImageFormat;
        depth_attachment.samples = _sampleCount;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment_ref = {};
        depth_attachment_ref.attachment = 2;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // As a MSAA image can't directly be displayed on the screen
        // some transition needs to be done.
        VkAttachmentDescription resolve_attachment {};
        resolve_attachment.flags = 0;
        resolve_attachment.format = _swapchain_image_format;
        resolve_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        resolve_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        resolve_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        resolve_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        resolve_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        resolve_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference resolve_attachment_ref {};
        resolve_attachment_ref.attachment = 1;
        resolve_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // single subpass
        VkSubpassDescription subpass {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment_ref;
        subpass.pDepthStencilAttachment = &depth_attachment_ref;
        subpass.pResolveAttachments = &resolve_attachment_ref;
        // array of 3 attachments, one for the color, and other for depth and one for
        // MSAA
        VkAttachmentDescription attachments[3];
        attachments[0] = color_attachment;
        attachments[1] = resolve_attachment;
        attachments[2] = depth_attachment;

        VkRenderPassCreateInfo render_pass_info {};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

        // connect the color && depth attachments
        render_pass_info.attachmentCount = 3;
        render_pass_info.pAttachments = &attachments[0];
        // connect the subpass
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;

        VK_CHECK(
                vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderpass));

        _swapchainDeletionQueue.push_function(
                [=]() { vkDestroyRenderPass(_device, _renderpass, nullptr); });
}

//  Init (framebuffers): Init the framebuffers
//      with the window properties
void VulkanEngine::init_framebuffers()
{
        // create frame buffers
        VkFramebufferCreateInfo frameBufferInfo {};
        frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frameBufferInfo.pNext = nullptr;

        // pretty simple
        frameBufferInfo.renderPass = _renderpass;
        frameBufferInfo.attachmentCount = 1;
        frameBufferInfo.width = _windowExtent.width;
        frameBufferInfo.height = _windowExtent.height;
        frameBufferInfo.layers = 1;

        // grab all images in da swapchain
        const uint32_t swapchain_imagecount = _swapchain_images.size();
        _frameBuffers = std::vector<VkFramebuffer>(swapchain_imagecount);

        // create framebuffers for each of the swapchain image views
        for (int i = 0; i < swapchain_imagecount; i++) {
                VkImageView attachments[3];
                attachments[0] = _resolveImageView;
                attachments[1] = _swapchain_image_views[i];
                attachments[2] = _depthImageView;

                frameBufferInfo.pAttachments = attachments;
                frameBufferInfo.attachmentCount = 3;
                VK_CHECK(vkCreateFramebuffer(_device, &frameBufferInfo, nullptr,
                        &_frameBuffers[i]));

                _swapchainDeletionQueue.push_function([=]() {
                        vkDestroyFramebuffer(_device, _frameBuffers[i], nullptr);
                        vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
                });
        }
}

//  Init (Sync Structures): Init semaphores and fences
void VulkanEngine::init_sync_structures()
{
        VkFenceCreateInfo fenceInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);

        VkFenceCreateInfo uploadFenceInfo {};
        uploadFenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        uploadFenceInfo.pNext = nullptr;

        for (int i = 0; i < FRAME_OVERLAP; i++) {

                VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i]._fence));
                VK_CHECK(vkCreateFence(_device, &uploadFenceInfo, nullptr,
                        &_uploadContext._uploadFence));

                _mainDeletionQueue.push_function([=]() {
                        vkDestroyFence(_device, _frames[i]._fence, nullptr);
                        vkDestroyFence(_device, _uploadContext._uploadFence, nullptr);
                });

                VkSemaphoreCreateInfo semaphoreInfo {};
                semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
                semaphoreInfo.pNext = nullptr;
                semaphoreInfo.flags = 0;

                VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                        &_frames[i]._renderSemaphore));
                VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                        &_frames[i]._presentSemaphore));

                _mainDeletionQueue.push_function([=]() {
                        vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
                        vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
                });
        }
}

//  Loader (Shader Module): Helper function to load the shader modules
//      and return them in the form of VkShaderModule(s)
//      in the second argument of the function
bool VulkanEngine::load_shader_module(const char* filepath,
        VkShaderModule* outshaderModule)
{
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
                return false;
        }

        // Find what the size of the file is by telling where the cursor is
        // as we used std::ios::ate, the cursor will be at the end of the file
        size_t fileSize = (size_t)file.tellg();
        std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

        // put the cursor back at the start of the file
        // as now we need to read it
        file.seekg(0);

        // Read them bytes
        file.read((char*)buffer.data(), fileSize);

        file.close();

        VkShaderModuleCreateInfo shader_module_info {};
        shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shader_module_info.pNext = nullptr;

        // Supply vulkan with information about the file
        shader_module_info.codeSize = buffer.size() * sizeof(uint32_t);
        shader_module_info.pCode = buffer.data();

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(_device, &shader_module_info, nullptr,
                    &shaderModule)
                != VK_SUCCESS) {
                return false;
        }

        *outshaderModule = shaderModule;
        return true;
}

//  Init (Pipeline): Init the rendering pipeline(s) with
//      everything that was setup
void VulkanEngine::init_pipelines()
{
        VkShaderModule meshVertShader, colorMeshShader;

        if (!load_shader_module("../shaders/compiled/tri_mesh.vert.spv",
                    &meshVertShader)) {
                std::cout << "Failed to create Mesh fragment shader." << std::endl;
        } else {
                std::cout << "Successfully created fragment shader." << std::endl;
        }

        if (!load_shader_module("../shaders/compiled/default_lit.frag.spv",
                    &colorMeshShader)) {
                std::cout << "Failed to create Mesh fragment shader." << std::endl;
        } else {
                std::cout << "Successfully created fragment shader." << std::endl;
        }

        // not using any desc. sets or anything so it's good for now.
        VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

        PipelineBuilder pipelineBuilder;

        // Create infos for vertex and fragment shader

        pipelineBuilder._shaderStages.push_back(
                vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                        colorMeshShader));

        // tells vulkan how to read vertices, isn't in use rn.
        pipelineBuilder._vertexInputInfo = vkinit::vertex_input_stage_create_info();

        // set polygon mode:
        // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : normal triangle drawing
        // VK_PRIMITIVE_TOPOLOGY_POINT_LIST    : points
        // VK_PRIMITIVE_TOPOLOGY_LINE_LIST     : line-list
        pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // Tell the rasterizer that we wanna fill the objects
        // Wireframe Mode: VK_POLYGON_MODE_LINE
        // Solid Mode: VK_POLYGON_MODE_FILL
        pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

        // MSAA
        pipelineBuilder._multisampling = vkinit::multisampling_state_create_info(_sampleCount);

        // color blending
        pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

        pipelineBuilder._depthStencilInfo = vkinit::depth_stencil_create_info(
                true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

        // clear the shader stages for the builder
        pipelineBuilder._shaderStages.clear();

        VertexInputDescription vertexDescription = Vertex::get_vertex_input_desc();

        // connect the pipeline builder vertex input info to the one we get from
        // Vertex
        pipelineBuilder._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
        pipelineBuilder._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();

        pipelineBuilder._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
        pipelineBuilder._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

        // remove the old shaders and make way for the new ones. Liberal moment.
        pipelineBuilder._shaderStages.clear();

        VkPushConstantRange pushConstantRange {};
        pushConstantRange.offset = 0; // first member in struct
        pushConstantRange.size = sizeof(MeshPushConstants);
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // because the mesh shader is a vertex shader

        // start with a default, empty layout info
        VkPipelineLayoutCreateInfo mesh_pipeline_layout_info = vkinit::pipeline_layout_create_info();

        mesh_pipeline_layout_info.pushConstantRangeCount = 1;
        mesh_pipeline_layout_info.pPushConstantRanges = &pushConstantRange;

        VkDescriptorSetLayout setLayouts[2] = { _globalSetLayout, _objectSetLayout };
        mesh_pipeline_layout_info.setLayoutCount = 2;
        mesh_pipeline_layout_info.pSetLayouts = setLayouts;

        // add the other shaders
        pipelineBuilder._shaderStages.push_back(
                vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,
                        meshVertShader));

        // make sure that colorMeshShader is holding the compiled
        // colored_triangle.frag
        pipelineBuilder._shaderStages.push_back(
                vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,
                        colorMeshShader));

        VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr,
                &_meshPipelineLayout));

        pipelineBuilder._pipelineLayout = _meshPipelineLayout;
        _meshPipeline = pipelineBuilder.build_pipeline(_device, _renderpass);

        create_material(_meshPipeline, _meshPipelineLayout, "defaultmaterial");

        // destroy all shader modules, outside of the queue
        vkDestroyShaderModule(_device, meshVertShader, nullptr);
        vkDestroyShaderModule(_device, colorMeshShader, nullptr);

        _mainDeletionQueue.push_function([=]() {
                // destroy the 2 pipelines we have created
                vkDestroyPipeline(_device, _meshPipeline, nullptr);

                // destroy the pipeline layout that they use
                vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
        });
}

//  Loader (Meshes): get the meshes, upload to the GPU and send to the scene
void VulkanEngine::load_meshes()
{
        // make the array 3 vertices long
        _triangleMesh._vertices.resize(3);

        // vertex positions
        _triangleMesh._vertices[0].position = { 0.5f, 0.5f, 0.0f };
        _triangleMesh._vertices[1].position = { -0.5f, 0.5f, 0.0f };
        _triangleMesh._vertices[2].position = { 0.f, -0.5f, 0.0f };

        // She's like a rainbow!
        _triangleMesh._vertices[0].color = { 0.1f, 1.0f, 0.1f };
        _triangleMesh._vertices[1].color = { 1.0f, 1.0f, 0.1f };
        _triangleMesh._vertices[2].color = { 0.1f, 0.1f, 1.0f };

        // we don't care about the vertex normals

        _carMesh.load_from_obj("../models/high_poly_suzanne.obj");
        _monkeyMesh.load_from_obj("../models/suzanne_2.obj");

        upload_mesh(_triangleMesh);
        upload_mesh(_monkeyMesh);
        upload_mesh(_carMesh);

        _meshes["monkey"] = _monkeyMesh;
        _meshes["triangle"] = _triangleMesh;
        _meshes["car"] = _carMesh;
}

//  Helper for Loader (Meshes): Upload the given mesh to the GPU memory
void VulkanEngine::upload_mesh(Mesh& mesh)
{
        const size_t bufferSize = mesh._vertices.size() * sizeof(Vertex);
        // allocate staging buffer
        VkBufferCreateInfo stagingBufferInfo {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.pNext = nullptr;

        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo vmaallocInfo {};
        vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        AllocatedBuffer stagingBuffer;

        VK_CHECK(vmaCreateBuffer(_allocator, &stagingBufferInfo, &vmaallocInfo,
                &stagingBuffer._buffer, &stagingBuffer._allocation,
                nullptr));

        void* data;
        vmaMapMemory(_allocator, stagingBuffer._allocation, &data);
        std::memcpy(data, mesh._vertices.data(),
                mesh._vertices.size() * sizeof(Vertex));
        vmaUnmapMemory(_allocator, stagingBuffer._allocation);

        VkBufferCreateInfo vertexBufferInfo {};
        vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertexBufferInfo.pNext = nullptr;

        vertexBufferInfo.size = bufferSize;

        vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        VK_CHECK(vmaCreateBuffer(_allocator, &vertexBufferInfo, &vmaallocInfo,
                &mesh._vertexBuffer._buffer,
                &mesh._vertexBuffer._allocation, nullptr));

        immediate_submit([=](VkCommandBuffer cmd) {
                VkBufferCopy copy;
                copy.dstOffset = 0;
                copy.srcOffset = 0;
                copy.size = bufferSize;
                vkCmdCopyBuffer(cmd, stagingBuffer._buffer, mesh._vertexBuffer._buffer, 1,
                        &copy);
        });

        // add the destruction of mesh buffer to the deletion queue
        _mainDeletionQueue.push_function([=]() {
                vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer,
                        mesh._vertexBuffer._allocation);
        });

        vmaDestroyBuffer(_allocator, stagingBuffer._buffer,
                stagingBuffer._allocation);
}

//  Helper (Materials): Create a material from the scene
Material* VulkanEngine::create_material(VkPipeline pipeline,
        VkPipelineLayout layout,
        const std::string& name)
{
        Material mat;
        mat.pipeline = pipeline;
        mat.pipelineLayout = layout;
        _materials[name] = mat;
        return &_materials[name];
}

//  Helper (Materials): Get a material from the scene
Material* VulkanEngine::get_material(const std::string& name)
{
        auto item = _materials.find(name);
        if (item == _materials.end())
                return nullptr;
        else
                return &(*item).second;
}

//  Helper (Meshes): Get a mesh from the scene
Mesh* VulkanEngine::get_mesh(const std::string& name)
{
        auto item = _meshes.find(name);
        if (item == _meshes.end())
                return nullptr;
        else
                return &(*item).second;
}

//  Helper (Scene)
void VulkanEngine::init_scene()
{
        RenderObject monkey;
        monkey.mesh = get_mesh("monkey");
        monkey.material = get_material("defaultmaterial");
        monkey.transformMatrix = glm::mat4 { 1.0f };

        glm::mat4 translation = glm::translate(glm::mat4 { 1.0f }, glm::vec3 { 0.0f, 2.0f, 0.0f });
        glm::mat4 scale = glm::scale(glm::mat4 { 1.0f }, glm::vec3(0.5f, 0.5f, 0.5f));

        monkey.transformMatrix = translation * scale;
        // yo me wanna render monke hoot hoot
        _renderables.push_back(monkey);

        RenderObject car;
        car.mesh = get_mesh("car");
        car.material = get_material("defaultmaterial");
        car.transformMatrix = glm::mat4 { 1.0f };

        _renderables.push_back(car);

        // apparently we create a lotta triangles in a grid and place them around the
        // monkee idfk how
        for (int x = -20; x <= 20; x++) {
                for (int y = -20; y <= 20; y++) {
                        RenderObject triangles;
                        triangles.mesh = get_mesh("triangle");
                        triangles.material = get_material("defaultmaterial");

                        // tbh i don't understand what any of the GLM shit does
                        // well now i do, so the x and y are positions for the triangles
                        // and there are currently 80 triangles here
                        // aka 80 drawcalls + that 1 for the mesh (it's unified)
                        glm::mat4 translation = glm::translate(glm::mat4 { 1.0f }, glm::vec3 { x, 0, y });
                        glm::mat4 scale = glm::scale(glm::mat4 { 1.0f }, glm::vec3(0.2f, 0.2f, 0.2f));

                        triangles.transformMatrix = translation * scale;

                        // illuminati moment + triangle moment + didn't ask + who asked ... and so
                        // on
                        _renderables.push_back(triangles);
                }
        }

        std::sort(_renderables.begin(), _renderables.end(),
                [](RenderObject a, RenderObject b) {
                        if (a.material == b.material) {
                                return true;
                        }
                        if (a.mesh == b.mesh) {
                                return true;
                        }
                        return false;
                });
}

//  Drawcall (Scene): Draw all the objects that are in provided to the provided
//  command buffer
void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first,
        int count)
{
        // make a model view martix for rendering the object
        // camerea view
        glm::vec3 cameraPos = (_camera_positions);
        glm::mat4 view = glm::translate(glm::mat4 { 1.0f }, cameraPos);

        // camera projection
        glm::mat4 projection = glm::perspective(
                45.0f, (float)_windowExtent.width / (float)_windowExtent.height, 0.1f,
                200.0f);

        // // make whatever this is a negative value, fuck knows why
        // // ohhh so i did this to fix the darn vulkan BS
        // i still do not know what this is
        projection[1][1] *= -1;
        // rotate only z axis
        glm::vec3 rotation_vector = glm::vec3(0, 1, 0);
        // todo: check out how glm::rotate actually works.
        glm::mat4 rotation = glm::rotate(_rotation, rotation_vector);

        // allocate the uniform buffer
        GPUCameraData cameraData;
        cameraData.projection = projection;
        cameraData.view = view;
        cameraData.rotation = rotation;
        cameraData.viewproj = projection * rotation * view;

        // aaaaand set it over
        void* data;
        vmaMapMemory(_allocator, get_current_frame().cameraBuffer._allocation, &data);
        memcpy(data, &cameraData, sizeof(GPUCameraData));
        vmaUnmapMemory(_allocator, get_current_frame().cameraBuffer._allocation);

        // After the camera buffer has been mapped, send the data to the
        float framed = (_frameNumber / 120.0f);

        _sceneParams.ambientColor = { sin(framed), 0, cos(framed), 1 };

        char* sceneData;
        vmaMapMemory(_allocator, _sceneParamsBuffer._allocation, (void**)&sceneData);
        int frameIndex = _frameNumber % FRAME_OVERLAP;
        sceneData += pad_uniform_buffer(sizeof(GPUSceneData)) * frameIndex;
        memcpy(sceneData, &_sceneParams, sizeof(GPUSceneData));
        vmaUnmapMemory(_allocator, _sceneParamsBuffer._allocation);

        void* objectData;
        vmaMapMemory(_allocator, get_current_frame().objectBuffer._allocation,
                &objectData);

        // SSBO = Shared Storage Buffer Object
        GPUObjectData* objectSSBO = (GPUObjectData*)objectData;

        for (int i = 0; i < count; i++) {
                RenderObject& object = first[i];
                objectSSBO[i].modelMatrix = object.transformMatrix;
        }

        vmaUnmapMemory(_allocator, get_current_frame().objectBuffer._allocation);

        Mesh* lastMesh = nullptr;
        Material* lastMaterial = nullptr;

        /*
        There is no need to rebind the same vertex buffer over and over between draws,
        and the pipeline is the same, but we are pushing the constants on every single
        call. The loop here is a lot higher performance that you would think. This
        simple loop will render thousands and thousands of objects with no issue.
        Binding pipeline is a expensive call, but drawing the same object over and
        over with different push constants is very fast.
            - VkGuide.
      */

        for (int i = 0; i < count; i++) {
                RenderObject& object = first[i];

                // only bind the pipeline if it doesnt match with the already bound one
                if (object.material != lastMaterial) {
                        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                object.material->pipeline);
                        lastMaterial = object.material;

                        uint32_t uniform_offset = pad_uniform_buffer(sizeof(GPUSceneData)) * frameIndex;
                        vkCmdBindDescriptorSets(
                                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
                                0, 1, &get_current_frame().globalDescriptorSet, 1, &uniform_offset);

                        // object data descriptor
                        vkCmdBindDescriptorSets(
                                cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout,
                                1, 1, &get_current_frame().objectDescriptorSet, 0, nullptr);
                }

                glm::mat4 model = object.transformMatrix;
                // final render matrix, that we are calculating on the cpu
                glm::mat4 mesh_matrix = model;

                MeshPushConstants constants;
                constants.render_matrix = mesh_matrix;

                // upload the mesh to the gpu via pushconstants
                vkCmdPushConstants(cmd, object.material->pipelineLayout,
                        VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants),
                        &constants);

                // only bind the mesh if its a different one from last bind
                if (object.mesh != lastMesh) {
                        // bind the mesh vertex buffer with offset 0
                        VkDeviceSize offset = 0;
                        vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer,
                                &offset);
                        lastMesh = object.mesh;
                }
                // we can now draw
                vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, i);
                _currentDrawCalls++;
        }
}

//  Helper (Multiple Buffers): use this to get the current frame
//      example: if you're double buffering, you can use this function to get
//      the number of the frame being rendered right now.
FrameData& VulkanEngine::get_current_frame()
{
        // Every time we render a frame, the _frameNumber gets bumped by 1.
        // This will be very useful here. With a frame overlap of 2 (the default),
        // it means that even frames will use _frames[0], while odd frames will use
        // _frames[1].
        return _frames[_frameNumber % FRAME_OVERLAP];
}

//  Helper (Allocator): Allocates a buffer with the given requirements.
AllocatedBuffer VulkanEngine::create_buffer(size_t allocsize,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage)
{
        VkBufferCreateInfo bufferInfo {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.pNext = nullptr;

        bufferInfo.size = allocsize;
        bufferInfo.usage = usage;

        VmaAllocationCreateInfo allocInfo {};
        allocInfo.usage = memoryUsage;

        AllocatedBuffer newBuffer;

        // Allocate the buffer
        VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo,
                &newBuffer._buffer, &newBuffer._allocation,
                nullptr));

        return newBuffer;
}

size_t VulkanEngine::pad_uniform_buffer(size_t originalSize)
{
        // Calculate required alignment based on minimum device offset alignment
        size_t minUboAlignment = _deviceProperties.limits.minUniformBufferOffsetAlignment;
        size_t alignedSize = originalSize;
        if (minUboAlignment > 0) {
                alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
        }
        return alignedSize;
}

// Init (Descriptors)
void VulkanEngine::init_descriptors()
{
        std::vector<VkDescriptorPoolSize> sizes {
                // gimme 10 uniform buffer descriptors bro
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }
        };

        VkDescriptorPoolCreateInfo descPoolInfo {};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.pNext = nullptr;

        descPoolInfo.maxSets = 10;
        descPoolInfo.flags = 0;

        descPoolInfo.poolSizeCount = (uint32_t)sizes.size();
        descPoolInfo.pPoolSizes = sizes.data();

        vkCreateDescriptorPool(_device, &descPoolInfo, nullptr, &_descriptorPool);

        VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, 0);
        VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorset_layout_binding(
                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);

        VkDescriptorSetLayoutBinding bindings[] = { cameraBind, sceneBind };

        VkDescriptorSetLayoutCreateInfo setinfo = {};
        setinfo.bindingCount = 2;
        setinfo.flags = 0;
        setinfo.pNext = nullptr;
        setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setinfo.pBindings = bindings;

        vkCreateDescriptorSetLayout(_device, &setinfo, nullptr, &_globalSetLayout);

        VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorset_layout_binding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                VK_SHADER_STAGE_VERTEX_BIT, 0);

        VkDescriptorSetLayoutCreateInfo setinfo2 {};
        setinfo2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        setinfo2.pNext = nullptr;
        setinfo2.bindingCount = 1; // object
        setinfo2.pBindings = &objectBind;
        setinfo2.flags = 0;

        vkCreateDescriptorSetLayout(_device, &setinfo2, nullptr, &_objectSetLayout);

        const size_t sceneParamBufferSize = FRAME_OVERLAP * pad_uniform_buffer(sizeof(GPUSceneData));
        _sceneParamsBuffer = create_buffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VMA_MEMORY_USAGE_CPU_TO_GPU);

        for (int i = 0; i < FRAME_OVERLAP; i++) {
                _frames[i].cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);

                const int MAX_OBJECTS = 10000;
                _frames[i].objectBuffer = create_buffer(sizeof(GPUObjectData) * MAX_OBJECTS,
                        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                        VMA_MEMORY_USAGE_CPU_TO_GPU);

                // Allocate the descriptor created for the current frame
                VkDescriptorSetAllocateInfo allocInfo {};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.pNext = nullptr;
                allocInfo.descriptorSetCount = 1;
                allocInfo.descriptorPool = _descriptorPool;
                allocInfo.pSetLayouts = &_globalSetLayout;

                vkAllocateDescriptorSets(_device, &allocInfo,
                        &_frames[i].globalDescriptorSet);

                // Allocate the descriptor created for the current frame
                VkDescriptorSetAllocateInfo objSetAlloc {};
                objSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                objSetAlloc.pNext = nullptr;
                objSetAlloc.descriptorSetCount = 1;
                objSetAlloc.descriptorPool = _descriptorPool;
                objSetAlloc.pSetLayouts = &_objectSetLayout;

                vkAllocateDescriptorSets(_device, &objSetAlloc,
                        &_frames[i].objectDescriptorSet);

                VkDescriptorBufferInfo cameraInfo;
                cameraInfo.buffer = _frames[i].cameraBuffer._buffer;
                cameraInfo.offset = 0;
                cameraInfo.range = sizeof(GPUCameraData);

                VkDescriptorBufferInfo sceneInfo;
                sceneInfo.buffer = _sceneParamsBuffer._buffer;
                sceneInfo.offset = 0;
                sceneInfo.range = sizeof(GPUSceneData);

                VkDescriptorBufferInfo objectInfo;
                objectInfo.buffer = _frames[i].objectBuffer._buffer;
                objectInfo.offset = 0;
                objectInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

                VkWriteDescriptorSet cameraWrite = vkinit::write_descriptor_buffer(
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, _frames[i].globalDescriptorSet,
                        &cameraInfo, 0);
                VkWriteDescriptorSet sceneWrite = vkinit::write_descriptor_buffer(
                        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
                        _frames[i].globalDescriptorSet, &sceneInfo, 1);
                VkWriteDescriptorSet objectWrite = vkinit::write_descriptor_buffer(
                        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, _frames[i].objectDescriptorSet,
                        &objectInfo, 0);

                VkWriteDescriptorSet setWrites[] = { cameraWrite, sceneWrite, objectWrite };

                vkUpdateDescriptorSets(_device, 3, setWrites, 0, nullptr);
        }

        _mainDeletionQueue.push_function([&]() {
                vmaDestroyBuffer(_allocator, _sceneParamsBuffer._buffer,
                        _sceneParamsBuffer._allocation);
                vkDestroyDescriptorSetLayout(_device, _objectSetLayout, nullptr);
                vkDestroyDescriptorSetLayout(_device, _globalSetLayout, nullptr);

                vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);

                for (int i = 0; i < FRAME_OVERLAP; i++) {
                        vmaDestroyBuffer(_allocator, _frames[i].cameraBuffer._buffer,
                                _frames[i].cameraBuffer._allocation);
                        vmaDestroyBuffer(_allocator, _frames[i].objectBuffer._buffer,
                                _frames[i].objectBuffer._allocation);
                }
        });
};

void VulkanEngine::immediate_submit(
        std::function<void(VkCommandBuffer)>&& function)
{
        // allocating the immediate command buffer
        VkCommandBufferAllocateInfo immediateCommandBufferInfo = vkinit::command_buffer_allocate_info(_uploadContext._commandPool);

        // Allocate the command buffer
        VkCommandBuffer cmd;
        VK_CHECK(
                vkAllocateCommandBuffers(_device, &immediateCommandBufferInfo, &cmd));

        // begin recording the command buffer
        VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
                VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

        // execute the function
        function(cmd);

        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submit = vkinit::sumbit_info(&cmd);

        // submit command buffer to the queue and execute it
        VK_CHECK(
                vkQueueSubmit(_graphicsQueue, 1, &submit, _uploadContext._uploadFence));

        vkWaitForFences(_device, 1, &_uploadContext._uploadFence, true, 9999999999);
        vkResetFences(_device, 1, &_uploadContext._uploadFence);

        // clear the command pool for the next immediate submit
        vkResetCommandPool(_device, _uploadContext._commandPool, 0);
}
