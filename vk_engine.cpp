#define VMA_IMPLEMENTATION

#include "vk_engine.h"

#include <fstream>
#include <iostream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "VkBootstrap.h"
#include <vk_initializers.h>
#include <vk_types.h>

#include <backends/imgui_impl_sdl.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

// TODO: Make function documentation better by specifying args and their purposes

/*
    The descriptor set number 0 will be used for engine-global resources, and bound once per frame. 
    The descriptor set number 1 will be used for per-pass resources, and bound once per pass. 
    The descriptor set number 2 will be used for material resources, 
    and the number 3 will be used for per-object resources. 
    This way, the inner render loops will only be binding descriptor sets 2 and 3, and performance will be high.
*/

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            std::cout << "Detected Vulkan error: " << err << std::endl; \
            abort();                                                    \
        }                                                               \
    } while (0)

//  Init (Main): SDL and all the vulkan components
void VulkanEngine::init()
{
    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    init_vulkan();
    init_swapchain();
    init_commands();
    init_default_renderpass();
    init_framebuffers();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    load_meshes();
    init_scene();

    //everything went fine
    _isInitialized = true;
}

//  Draw: Called every frame, drawcall
void VulkanEngine::draw()
{
    // that changes now.
    VK_CHECK(vkWaitForFences(_device, 1, &get_current_frame()._fence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &get_current_frame()._fence));

    //request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame()._presentSemaphore, nullptr, &swapchainImageIndex));
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

    // make a clear-color from frame number. This will flash with a 120*pi frame period.
    VkClearValue clearValue;
    float flash = abs(sin(_frameNumber / 120.f));
    clearValue.color = { { 0.0f, 0.0f, flash, 1.0f } };

    VkClearValue depthClear;
    depthClear.depthStencil.depth = 1.f;

    // start the main renderpass.
    // We will use the clear color from above, and the framebuffer of the index the swapchain gave us
    VkRenderPassBeginInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.pNext = nullptr;

    rpInfo.renderPass = _renderpass;
    rpInfo.renderArea.offset.x = 0;
    rpInfo.renderArea.offset.y = 0;
    rpInfo.renderArea.extent = _windowExtent;
    rpInfo.framebuffer = _frameBuffers[swapchainImageIndex];

    // connect clear values
    rpInfo.clearValueCount = 2;
    VkClearValue clearValues[] = { clearValue, depthClear };
    rpInfo.pClearValues = &clearValues[0];

    // begin the render pass
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // i'll just keep this for the keks, kekw:
    // record???? wtf??? where??? yes now i know, because we didn't have the pipeline back then
    // now. we. do.

    draw_objects(cmd, _renderables.data(), _renderables.size());
    // finalize and end the render pass
    vkCmdEndRenderPass(cmd);

    // finalize the command buffer
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare for your ass to get shipped
    // wait for the semaphore to say, "alright mate come in, Dr. swapchain is ready to check yo ass"
    // wait for the second semaphore to say, "alright mate, the wait is over; Dr. rendering has finished the surgery"
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
    VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, get_current_frame()._fence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &get_current_frame()._renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    //increase the number of frames drawn
    _frameNumber++;
}

//  Run: SDL stuff
void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    //main loop
    while (!bQuit) {
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            //close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;
            else if (e.type == SDL_KEYDOWN) {
                // Left-Right
                if (e.key.keysym.sym == SDLK_a) {
                    _camera_positions.x += 0.1f;
                } else if (e.key.keysym.sym == SDLK_d) {
                    _camera_positions.x -= 0.1f;
                }
                // Front-Back
                else if (e.key.keysym.sym == SDLK_w) {
                    _camera_positions.z += 0.1f;
                } else if (e.key.keysym.sym == SDLK_s) {
                    _camera_positions.z -= 0.1f;
                }
                // Up-Down
                else if (e.key.keysym.sym == SDLK_q) {
                    _camera_positions.y += 0.1f;
                } else if (e.key.keysym.sym == SDLK_e) {
                    _camera_positions.y -= 0.1f;
                }
                // Rotate right-left
                else if (e.key.keysym.sym == SDLK_x) {
                    _rotation += 0.1f;
                } else if (e.key.keysym.sym == SDLK_z) {
                    _rotation -= 0.1f;
                }
            }
        }

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

    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });
}

//  Init (Swapchain): Init the swapchain and the memory allocator
void VulkanEngine::init_swapchain()
{
    vkb::SwapchainBuilder swapchainBuilder { _chosen_GPU, _device, _surface };
    vkb::Swapchain vkbSwapchain = swapchainBuilder.use_default_format_selection()
                                      .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                      .set_desired_extent(_windowExtent.width, _windowExtent.height)
                                      .build()
                                      .value();

    // store the swapchain and it's related images
    _swapchain = vkbSwapchain.swapchain;
    _swapchain_images = vkbSwapchain.get_images().value();
    _swapchain_image_views = vkbSwapchain.get_image_views().value();

    _swapchain_image_format = vkbSwapchain.image_format;

    _mainDeletionQueue.push_function([=] {
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    });

    // make the size match the window, because obviously
    // we don't want a depth buffer smaller/larger than the viewport
    // because that's fucking stupid (at least afaik)
    VkExtent3D depthImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    // hardcoding the format to 32 bit float
    _depthImageFormat = VK_FORMAT_D32_SFLOAT;

    // same depth format usage flag
    VkImageCreateInfo dimg_info = vkinit::create_image_info(_depthImageFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

    // for the depth image, we want to alloc it from the GPU's memory
    VmaAllocationCreateInfo depth_buffer_allocation_info {};
    depth_buffer_allocation_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    depth_buffer_allocation_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // alloc and create the image
    vmaCreateImage(_allocator, &dimg_info, &depth_buffer_allocation_info, &_depthImage._image, &_depthImage._allocation, nullptr);

    // build an image view for the depth buffer to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::create_image_view_info(_depthImageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_device, &dview_info, nullptr, &_depthImageView));

    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_device, _depthImageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
    });
}

//  Cleanup: Run the deletion queue and general cleanup
void VulkanEngine::cleanup()
{
    // pretty self explainatory
    if (_isInitialized) {
        // wait till the GPU is done doing it's thing
        vkDeviceWaitIdle(_device);

        _mainDeletionQueue.flush();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vkDestroyDevice(_device, nullptr);
        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);

        SDL_DestroyWindow(_window);
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
            and being consumed by the GPU, at this point it is NOT safe to reset the command buffer yet.
            You need to make sure that the GPU has finished executing
            all of the commands from that command buffer until you can reset and reuse it.

        // btw pools are created and command buffers are allocated to the pools.
    */
    // step 1: create a command pool with the following specifications
    // no need to put this in the for loop
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_frames[i]._commandPool));

        // step 2: create commandBuffers
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);
        VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_frames[i]._mainCommandBuffer));

        _mainDeletionQueue.push_function([=]() {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);
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
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // clear the renderpass if the attachment when loaded
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // keep the attachmant stored when da renderpass be goin home
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // we don't care about the stencil
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // we don't know or care about the starting layout of the image attachment
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // we use the KHR layout (Image is on a layout that allows displaying the image on the screen)
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // Now that our main image target is defined, we need to add a subpass that will render into it.
    VkAttachmentReference color_attachment_ref {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Image is on a layout optimal to be written into by rendering commands.

    VkAttachmentDescription depth_attachment {};
    depth_attachment.flags = 0;
    depth_attachment.format = _depthImageFormat;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // single subpass
    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    //array of 2 attachments, one for the color, and other for depth
    VkAttachmentDescription attachments[2] = { color_attachment, depth_attachment };

    VkRenderPassCreateInfo render_pass_info {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

    // connect the color && depth attachments
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = &attachments[0];
    // connect the subpass
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderpass));

    _mainDeletionQueue.push_function([=]() {
        vkDestroyRenderPass(_device, _renderpass, nullptr);
    });
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
        VkImageView attachments[2];
        attachments[0] = _swapchain_image_views[i];
        attachments[1] = _depthImageView;

        frameBufferInfo.pAttachments = attachments;
        frameBufferInfo.attachmentCount = 2;
        VK_CHECK(vkCreateFramebuffer(_device, &frameBufferInfo, nullptr, &_frameBuffers[i]));

        _mainDeletionQueue.push_function([=]() {
            vkDestroyFramebuffer(_device, _frameBuffers[i], nullptr);
            vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
        });
    }
}

//  Init (Sync Structures): Init semaphores and fences
void VulkanEngine::init_sync_structures()
{
    // create seamen (semaphores)
    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;

    // wait on it before using it on a GPU command for the first time
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_frames[i]._fence));

        _mainDeletionQueue.push_function([=]() {
            vkDestroyFence(_device, _frames[i]._fence, nullptr);
        });

        VkSemaphoreCreateInfo semaphoreInfo {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphoreInfo.pNext = nullptr;
        semaphoreInfo.flags = 0;

        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i]._renderSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_frames[i]._presentSemaphore));

        _mainDeletionQueue.push_function([=]() {
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._presentSemaphore, nullptr);
        });
    }
}

//  Loader (Shader Module): Helper function to load the shader modules
//      and return them in the form of VkShaderModule(s)
//      in the second argument of the function
bool VulkanEngine::load_shader_module(const char* filepath, VkShaderModule* outshaderModule)
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
    if (vkCreateShaderModule(_device, &shader_module_info, nullptr, &shaderModule) != VK_SUCCESS) {
        return false;
    }

    *outshaderModule = shaderModule;
    return true;
}

//  Init (Pipeline): Init the rendering pipeline(s) with
//      everything that was setup
void VulkanEngine::init_pipelines()
{
    VkShaderModule triangleFragShader, meshVertShader;

    if (!load_shader_module("../shaders/compiled/triangle_colored.frag.spv", &triangleFragShader)) {
        std::cout << "Failed to create fragment shader." << std::endl;
    } else {
        std::cout << "Successfully created fragment shader." << std::endl;
    }

    if (!load_shader_module("../shaders/compiled/tri_mesh.vert.spv", &meshVertShader)) {
        std::cout << "Failed to create Mesh fragment shader." << std::endl;
    } else {
        std::cout << "Successfully created fragment shader." << std::endl;
    }

    // not using any desc. sets or anything so it's good for now.
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

    PipelineBuilder pipelineBuilder;

    // Create infos for vertex and fragment shader

    pipelineBuilder._shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    // tells vulkan how to read vertices, isn't in use rn.
    pipelineBuilder._vertexInputInfo = vkinit::vertex_input_stage_create_info();

    // set polygon mode:
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : normal triangle drawing
    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST    : points
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST     : line-list
    pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    //build viewport and scissor from the swapchain extents
    pipelineBuilder._viewport.x = 0.0f;
    pipelineBuilder._viewport.y = 0.0f;
    pipelineBuilder._viewport.width = (float)_windowExtent.width;
    pipelineBuilder._viewport.height = (float)_windowExtent.height;
    pipelineBuilder._viewport.minDepth = 0.0f;
    pipelineBuilder._viewport.maxDepth = 1.0f;

    pipelineBuilder._scissor.offset = { 0, 0 };
    pipelineBuilder._scissor.extent = _windowExtent;

    // Tell the rasterizer that we wanna fill the objects
    // Wireframe Mode: VK_POLYGON_MODE_LINE
    // Solid Mode: VK_POLYGON_MODE_FILL
    pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    // MSAA
    pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

    // color blending
    pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

    pipelineBuilder._depthStencilInfo = vkinit::depth_stencil_create_info(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

    //clear the shader stages for the builder
    pipelineBuilder._shaderStages.clear();

    VertexInputDescription vertexDescription = Vertex::get_vertex_input_desc();

    // connect the pipeline builder vertex input info to the one we get from Vertex
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

    mesh_pipeline_layout_info.setLayoutCount = 1;
    mesh_pipeline_layout_info.pSetLayouts = &_globalSetLayout;

    // add the other shaders
    pipelineBuilder._shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));

    // make sure that triangleFragShader is holding the compiled colored_triangle.frag
    pipelineBuilder._shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader));

    VK_CHECK(vkCreatePipelineLayout(_device, &mesh_pipeline_layout_info, nullptr, &_meshPipelineLayout));

    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    _meshPipeline = pipelineBuilder.build_pipeline(_device, _renderpass);

    create_material(_meshPipeline, _meshPipelineLayout, "defaultmaterial");

    //destroy all shader modules, outside of the queue
    vkDestroyShaderModule(_device, meshVertShader, nullptr);
    vkDestroyShaderModule(_device, triangleFragShader, nullptr);

    _mainDeletionQueue.push_function([=]() {
        //destroy the 2 pipelines we have created
        vkDestroyPipeline(_device, _meshPipeline, nullptr);

        //destroy the pipeline layout that they use
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

    _monkeyMesh.load_from_obj("../models/audi.obj");
    upload_mesh(_triangleMesh);
    upload_mesh(_monkeyMesh);

    _meshes["monkey"] = _monkeyMesh;
    _meshes["triangle"] = _triangleMesh;
}

//  Helper for Loader (Meshes): Upload the given mesh to the GPU memory
void VulkanEngine::upload_mesh(Mesh& mesh)
{
    VkBufferCreateInfo bufferInfo {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.size = mesh._vertices.size() * sizeof(Vertex);

    // Create a buffer that is writeable by the CPU and readable by the GPU
    VmaAllocationCreateInfo vmallocInfo {};
    vmallocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmallocInfo, &mesh._vertexBuffer._buffer, &mesh._vertexBuffer._allocation, nullptr));

    _mainDeletionQueue.push_function([=]() {
        vmaDestroyBuffer(_allocator, mesh._vertexBuffer._buffer, mesh._vertexBuffer._allocation);
    });

    // get the mapped memory from VMA in the form of data
    void* data;
    vmaMapMemory(_allocator, mesh._vertexBuffer._allocation, &data);
    // copy the data from the vertices' data to the one mapped by VMA
    memcpy(data, mesh._vertices.data(), mesh._vertices.size() * sizeof(Vertex));
    // Unmap the data as we aren't streaming it
    vmaUnmapMemory(_allocator, mesh._vertexBuffer._allocation);
}

//  Helper (Materials): Create a material from the scene
Material* VulkanEngine::create_material(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
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

    // yo me wanna render monke hoot hoot
    _renderables.push_back(monkey);

    // apparently we create a lotta triangles in a grid and place them around the monkee
    // idfk how
    for (int x = -20; x <= 20; x++) {
        for (int y = -20; y <= 20; y++) {
            RenderObject triangles;
            triangles.mesh = get_mesh("triangle");
            triangles.material = get_material("defaultmaterial");

            // tbh i don't understand what any of the GLM shit does
            glm::mat4 translation = glm::translate(glm::mat4 { 1.0f }, glm::vec3 { x, 0, y });
            glm::mat4 scale = glm::scale(glm::mat4 { 1.0f }, glm::vec3(0.2f, 0.2f, 0.2f));

            triangles.transformMatrix = translation * scale;

            // illuminati moment + triangle moment + didn't ask + who asked ... and so on
            _renderables.push_back(triangles);
        }
    }
}

//  Drawcall (Scene): Draw all the objects that are in provided to the provided command buffer
void VulkanEngine::draw_objects(VkCommandBuffer cmd, RenderObject* first, int count)
{
    // make a model view martix for rendering the object
    // camerea view
    glm::vec3 cameraPos = (_camera_positions);
    glm::mat4 view = glm::translate(glm::mat4 { 1.0f }, cameraPos);

    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float)_windowExtent.width / (float)_windowExtent.height, 0.1f, 200.0f);

    // // make whatever this is a negative value, fuck knows why
    // // ohhh so i did this to fix the darn vulkan BS
    // i still do not know what this is
    projection[1][1] *= -1;
    // rotate only z axis
    glm::vec3 rotation_vector = glm::vec3(0, 1, 0);
    //todo: check out how glm::rotate actually works.
    glm::mat4 rotation = glm::rotate(_rotation, rotation_vector);
    std::cout << rotation_vector.x << '\t' << rotation_vector.y << '\t' << rotation_vector.z << '\t' << std::endl;

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

    Mesh* lastMesh = nullptr;
    Material* lastMaterial = nullptr;

    /*
        There is no need to rebind the same vertex buffer over and over between draws,
        and the pipeline is the same, but we are pushing the constants on every single call.
        The loop here is a lot higher performance that you would think.
        This simple loop will render thousands and thousands of objects with no issue.
        Binding pipeline is a expensive call, but drawing the same object over and over with different push constants is very fast.
            - VkGuide.
    */

    for (int i = 0; i < count; i++) {
        RenderObject& object = first[i];

        // only bind the pipeline if one hasn't already been bound.
        if (object.material != lastMaterial) {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
            lastMaterial = object.material;
            // Bind the desc set when changing the pipeline
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &get_current_frame().globalDescriptorSet, 0, nullptr);
        }

        glm::mat4 model = object.transformMatrix;
        // Find render matrix, calculated on the CPU for some fuckin reason.
        glm::mat4 mesh_matrix = projection * view * model;

        MeshPushConstants constants;
        constants.render_matrix = object.transformMatrix;

        // upload the mesh to the GPU via push constants
        vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

        if (object.mesh != lastMesh) {
            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->_vertexBuffer._buffer, &offset);
            lastMesh = object.mesh;
        }

        // btw 1 there means that we need 1 instance of that draw call
        vkCmdDraw(cmd, object.mesh->_vertices.size(), 1, 0, 0);
    }
}

//  Helper (Multiple Buffers): use this to get the current frame
//      example: if you're double buffering, you can use this function to get
//      the number of the frame being rendered right now.
FrameData& VulkanEngine::get_current_frame()
{
    // Every time we render a frame, the _frameNumber gets bumped by 1.
    // This will be very useful here. With a frame overlap of 2 (the default),
    // it means that even frames will use _frames[0], while odd frames will use _frames[1].
    return _frames[_frameNumber % FRAME_OVERLAP];
}

//  Helper (Allocator): Allocates a buffer with the given requirements.
AllocatedBuffer VulkanEngine::create_buffer(size_t allocsize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
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
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &allocInfo, &newBuffer._buffer, &newBuffer._allocation, nullptr));

    _mainDeletionQueue.push_function([&] {
        vmaDestroyBuffer(_allocator, newBuffer._buffer, newBuffer._allocation);
    });

    return newBuffer;
}

// Init (Descriptors)
void VulkanEngine::init_descriptors()
{
    std::vector<VkDescriptorPoolSize> sizes {
        // gimme 10 uniform buffer descriptors bro
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 }
    };

    VkDescriptorPoolCreateInfo descPoolInfo {};
    descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descPoolInfo.pNext = nullptr;

    descPoolInfo.maxSets = 10;
    descPoolInfo.flags = 0;

    descPoolInfo.poolSizeCount = (uint32_t)sizes.size();
    descPoolInfo.pPoolSizes = sizes.data();

    vkCreateDescriptorPool(_device, &descPoolInfo, nullptr, &_descriptorPool);

    VkDescriptorSetLayoutBinding cameraBinding {};
    // the position to bind to in the description set
    cameraBinding.binding = 0;
    // gimme 1 of those please
    cameraBinding.descriptorCount = 1;
    // being used for a uniform buffer
    cameraBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    // Getting used in the vertex shader
    cameraBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo descSetLayoutInfo {};
    descSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutInfo.pNext = nullptr;

    // How many were there? (bindings)
    descSetLayoutInfo.bindingCount = 1;
    // which one to use
    descSetLayoutInfo.pBindings = &cameraBinding;
    // no flags
    descSetLayoutInfo.flags = 0;

    vkCreateDescriptorSetLayout(_device, &descSetLayoutInfo, nullptr, &_globalSetLayout);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        _frames[i].cameraBuffer = create_buffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        // Allocate the descriptor created for the current frame
        VkDescriptorSetAllocateInfo allocInfo {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = nullptr;

        allocInfo.descriptorSetCount = 1;
        allocInfo.descriptorPool = _descriptorPool;

        allocInfo.pSetLayouts = &_globalSetLayout;

        vkAllocateDescriptorSets(_device, &allocInfo, &_frames[i].globalDescriptorSet);

        // Info about the buffer we want to point at in the descriptor
        VkDescriptorBufferInfo bufferInfo {};
        bufferInfo.buffer = _frames[i].cameraBuffer._buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(GPUCameraData);

        VkWriteDescriptorSet setWrite {};
        setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        setWrite.pNext = nullptr;

        setWrite.descriptorCount = 1;
        setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

        setWrite.pBufferInfo = &bufferInfo;
        setWrite.dstBinding = 0;
        setWrite.dstSet = _frames[i].globalDescriptorSet;

        vkUpdateDescriptorSets(_device, 1, &setWrite, 0, nullptr);
    }
};
