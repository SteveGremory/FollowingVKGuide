
#include "vk_engine.h"

#include <iostream>
#include <fstream>

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

#include "VkBootstrap.h"

#define VK_CHECK(x)                                                     \
    do                                                                  \
    {                                                                   \
        VkResult err = x;                                               \
        if (err)                                                        \
        {                                                               \
            std::cout <<"Detected Vulkan error: " << err << std::endl;  \
            abort();                                                    \
        }                                                               \
    } while (0)

void VulkanEngine::init()
{
    // We initialize SDL and create a window with it. 
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    
    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags
    );

    init_vulkan();
    init_swapchain();
    init_commands();
    init_default_renderpass();
    init_framebuffers();
    init_sync_structures();
    init_pipelines();

    //everything went fine
    _isInitialized = true;
}

void VulkanEngine::draw()
{
    ////nothing yet
    // that changes now.
    VK_CHECK(vkWaitForFences(_device, 1, &_render_fence, true, 1000000000));
    VK_CHECK(vkResetFences(_device, 1, &_render_fence));

    //request image from the swapchain, one second timeout
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, _present_semaphore, nullptr, &swapchainImageIndex)); 
    VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));
    
    VkCommandBuffer cmd = _mainCommandBuffer;
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
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    // begin the render pass
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    
    // record???? wtf??? where??? yes now i know, because we didn't have the pipeline back then
    // now. we. do.
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
    vkCmdDraw(cmd, 3, 1, 0 , 0);

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
    submitInfo.pWaitSemaphores = &_present_semaphore;

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_render_semaphore;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    // submit the queue and execute it
    // renderfence will now block everything until this shit finishes shitting
    VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, _render_fence));

    // this will put the image we just rendered into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as it's necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = nullptr;

    presentInfo.pSwapchains = &_swapchain;
    presentInfo.swapchainCount = 1;

    presentInfo.pWaitSemaphores = &_render_semaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;

    VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

    //increase the number of frames drawn
    _frameNumber++;

}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    //main loop
    while (!bQuit)
    {
        //Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            //close the window when user alt-f4s or clicks the X button			
            if (e.type == SDL_QUIT) bQuit = true;
        }

        draw();
    }
}

void VulkanEngine::init_vulkan() {
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
}

void VulkanEngine::init_swapchain() {
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
}

void VulkanEngine::cleanup() {
    // pretty self explainatory
    if (_isInitialized) {
        vkDestroyCommandPool(_device, _commandPool, nullptr);

        vkDestroyInstance(_instance, nullptr);

        vkDestroySwapchainKHR(_device, _swapchain, nullptr);

        vkDestroyRenderPass(_device, _renderpass, nullptr);

        for (int i = 0; _frameBuffers.size(); i++) {
            vkDestroyFramebuffer(_device, _frameBuffers[i], nullptr);
            vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
        }

        for (int i = 0; i < _swapchain_image_views.size(); i++) {
            vkDestroyImageView(_device, _swapchain_image_views[i], nullptr);
        }

        vkDestroyDevice(_device, nullptr);
        vkDestroySurfaceKHR(_instance, _surface, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        SDL_DestroyWindow(_window);

    }
}

void VulkanEngine::init_commands() {
    /*
        So here's how the commandBuffers work:
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
    VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

    // step 2: create commandBuffers
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));
}

void VulkanEngine::init_default_renderpass() {
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

    // single subpass
    VkSubpassDescription subpass {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkRenderPassCreateInfo render_pass_info {};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    
    // connect the color attachment
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    // connect the subpass
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderpass));

}

void VulkanEngine::init_framebuffers() {
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
        frameBufferInfo.pAttachments = &_swapchain_image_views[i];
        VK_CHECK(vkCreateFramebuffer(_device, &frameBufferInfo, nullptr, &_frameBuffers[i]));
    }
}

void VulkanEngine::init_sync_structures() {
    // create seamen (semaphores)
    VkFenceCreateInfo fenceInfo {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.pNext = nullptr;

    // wait on it before using it on a GPU command for the first time
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(_device, &fenceInfo, nullptr, &_render_fence));

    VkSemaphoreCreateInfo semaphoreInfo {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = nullptr;
    semaphoreInfo.flags = 0;

    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_render_semaphore));
    VK_CHECK(vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &_present_semaphore));
}

bool VulkanEngine::load_shader_module(const char* filepath, VkShaderModule* outshaderModule) {
    std::ifstream file (filepath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Find what the size of the file is by telling where the cursor is
    // as we used std::ios::ate, the cursor will be at the end of the file
    size_t fileSize = (size_t) file.tellg();
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

void VulkanEngine::init_pipelines() {
    VkShaderModule triangleFragShader;
    if (!load_shader_module("../shaders/triangle.frag.spv", &triangleFragShader))
    { std::cout << "Failed to create fragment shader." << std::endl; } else
    { std::cout << "Successfully created fragment shader." << std::endl; }

    VkShaderModule triangleVertShader;
    if (!load_shader_module("../shaders/triangle.vert.spv", &triangleVertShader))
    { std::cout << "Failed to create vertex shader." << std::endl; } else
    { std::cout << "Successfully created vertex shader." << std::endl; }

    // not using any desc. sets or anything so it's good for now.
    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();

    VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

    PipelineBuilder pipelineBuilder;

    // Create infos for vertex and fragment shader
    pipelineBuilder._shaderStages.push_back(
        vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertShader));

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
    pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

    // MSAA
    pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

    // color blending
    pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

    // pipeline layout
    pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

    // finally.
    _trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderpass);

}