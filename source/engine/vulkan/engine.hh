﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "imgui_internal.h"
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include "VkBootstrap.h"
#include "vk_mem_alloc.h"

#include "mesh.hh"
#include "types.hh"

#include <deque>
#include <functional>
#include <glm/glm.hpp>
#include <iostream>
#include <unordered_map>
#include <vector>

#define VK_CHECK(x)                                                            \
    do {                                                                       \
        VkResult err = x;                                                      \
        if (err) {                                                             \
            std::cout << "Detected Vulkan error: " << err << std::endl;        \
            abort();                                                           \
        }                                                                      \
    } while (0)

constexpr unsigned int FRAME_OVERLAP = 2;

struct MeshPushConstants {
    glm::vec4 data;
    glm::mat4 render_matrix;
};

struct DeletionQueue {
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) {
        deletors.push_back(function);
    };

    bool flush() {
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
            (*it)(); // call the function???? bro c++ moment
        }
        deletors.clear();
        return true;
    }
};

struct PipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
    VkPipelineVertexInputStateCreateInfo _vertexInputInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
    VkPipelineRasterizationStateCreateInfo _rasterizer;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo _multisampling;
    VkPipelineLayout _pipelineLayout;
    VkPipelineDepthStencilStateCreateInfo _depthStencilInfo;
    std::vector<VkDynamicState> _dynamicStateEnables;
    VkPipelineDynamicStateCreateInfo _dynamicStateInfo{};

    VkPipeline build_pipeline(VkDevice device, VkRenderPass pass);
};

struct Material {
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct RenderObject {
    Mesh* mesh;
    Material* material;
    glm::mat4 transformMatrix;
};

struct GPUCameraData {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 viewproj;
    glm::mat4 rotation;
};

struct GPUSceneData {
    glm::vec4 fogColor, // w is for exponent
        fogDistances,   // x for min y for max; z and w aren't being used
        ambientColor,
        sunlightDirection, // w for sun power
        sunlightColor;
};

struct GPUObjectData {
    glm::mat4 modelMatrix;
};

struct UploadContext {
    VkFence _uploadFence;
    VkCommandPool _commandPool;
};

struct FrameData {
    VkSemaphore _presentSemaphore, _renderSemaphore;
    VkFence _fence;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;

    // Allocated buffer that holds a singleG GPUCameraData
    AllocatedBuffer cameraBuffer;
    VkDescriptorSet globalDescriptorSet;

    AllocatedBuffer objectBuffer;
    VkDescriptorSet objectDescriptorSet;
};

class VulkanEngine {
public:
    //
    // Public Variables:
    //
    // Setup the low level stuff
    VkInstance _instance;                      // Vulkan Library Handle
    VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug output handle
    VkPhysicalDevice _chosen_GPU;              // "The chosen one- i mean GPU."
    VkDevice _device; // basically a handle to interact with the the GPU driver
    VkSurfaceKHR _surface; // the window's surface

    // Setup the swapchain
    VkSwapchainKHR _swapchain;        // make a swapchain
    VkFormat _swapchain_image_format; // image format being used by the platform
    std::vector<VkImage> _swapchain_images; // images generated by the swapchain
    std::vector<VkImageView>
        _swapchain_image_views; // image-views generated by the swapchain

    // CommandBuffer
    FrameData _frames[FRAME_OVERLAP]; // Frame Storage
    VkQueue _graphicsQueue; // graphicsqueue to supply those commands to the GPU
    uint32_t _graphicsQueueFamily; // specify the family of the queue

    // RenderPass
    VkRenderPass _renderpass; // you need a renderpass to display images from
                              // the commandbuffer
    std::vector<VkFramebuffer> _frameBuffers; // all the framebuffers that need
                                              // to be rendered to the screen

    // Deletion queue so that Vulkan Validation layers stop crying
    DeletionQueue _mainDeletionQueue; // jk it's so that every acquired resource
                                      // is deleted.
    DeletionQueue _swapchainDeletionQueue;

    // VulkanMemoryAllocator
    VmaAllocator _allocator;

    // Push Constants m8
    VkPipelineLayout _meshPipelineLayout;

    // suzanne moment -> rotating triangle moment
    VkPipeline _meshPipeline;
    Mesh _triangleMesh;

    // this time suzanne moment fr
    Mesh _monkeyMesh;
    // AudiMesh :dab:
    Mesh _carMesh;

    // Depth buffer moment :dab:
    VkImageView _depthImageView;
    AllocatedImage _depthImage;
    // Format of the depth image
    VkFormat _depthImageFormat;

    // MSAA
    VkImageView _resolveImageView;
    AllocatedImage _resolveImage;

    // Samples for MSAA
    VkSampleCountFlagBits _sampleCount;

    // Descriptor set pool
    VkDescriptorPool _descriptorPool;
    // Descriptor set layout
    VkDescriptorSetLayout _globalSetLayout;
    VkDescriptorSetLayout _objectSetLayout;
    // Physical Device Properties
    VkPhysicalDeviceProperties _deviceProperties;

    // GPU Scene Data and it's buffer (Desc Sets)
    GPUSceneData _sceneParams;
    AllocatedBuffer _sceneParamsBuffer;

    vkb::Swapchain _vkbSwapchain;
    VkSwapchainKHR _oldSwapChain;
    // Upload context for writing to a shared buffer between the GPU and the CPU
    UploadContext _uploadContext;

    // Objects to be rendered
    std::vector<RenderObject> _renderables;
    // Hashmap of materials
    std::unordered_map<std::string, Material> _materials;
    // Hashmap of meshes
    std::unordered_map<std::string, Mesh> _meshes;

    // SDL related variables
    bool _isInitialized{false};
    bool _wasResized{false};
    int _frameNumber{0};
    int _selectedShader{0};
    VkExtent2D _windowExtent{1280, 747};
    struct SDL_Window* _window{nullptr};

    glm::vec3 _camera_positions;
    float _rotation = 0.0f;
    double _fps = 0.0f;
    int _currentDrawCalls = 0;

    //
    // Public Functions:
    //
    // initializes everything in the engine
    void init();
    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();
    // Draw ImGUI UI
    void draw_stats();
    // Draw objects
    void draw_objects(VkCommandBuffer cmd, RenderObject* first, int count);
    // Getter for the frame currenting getting rendered.
    FrameData& get_current_frame();
    // Immediately create and submit a command buffer
    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    // run main loop
    void run();

    // load the shader module from the filepath
    bool load_shader_module(const char* filepath,
                            VkShaderModule* outshaderModule);
    // get them models
    void load_meshes();
    // upload the meshes to the pipeline
    void upload_mesh(Mesh& mesh);

    // Create material
    Material* create_material(VkPipeline pipeline, VkPipelineLayout layout,
                              const std::string& name);
    // Get the material from the hashmap, returns nullptr if it isn't found.
    Material* get_material(const std::string& name);
    // Get mesh; returns nullptr if the material isn't found.
    Mesh* get_mesh(const std::string& name);
    // Create a (general) buffer
    AllocatedBuffer create_buffer(size_t allocsize, VkBufferUsageFlags usage,
                                  VmaMemoryUsage memoryUsage);

    // Get the max sample count available
    VkSampleCountFlagBits get_max_usable_sample_count();
    // For padding the uniform buffer to make it the right size.
    size_t pad_uniform_buffer(size_t originalSize);
    // Recreate Swapchain
    void recreate_swapchain();

private:
    // Init low level vulkan stuff
    void init_vulkan();
    // Init the swapchain
    void init_swapchain(bool setOld);
    // Init the commandbuffer
    void init_commands();
    // Init the default renderpass
    void init_default_renderpass();
    // Init the framebuffers
    void init_framebuffers();
    // Init sync structures
    void init_sync_structures();
    // Init vulkan pipelines
    void init_pipelines();
    // Init the scene
    void init_scene();
    // Init descriptors
    void init_descriptors();
    // Init ImGUI
    void init_imgui();
};
