#pragma once
#include "asset-bundler/model.h"
#include <vulkan/vulkan.hpp>
#include "egg/renderer/memory.h"

struct process_job {
    vk::Device device;
    vk::UniqueCommandBuffer cmd_buffer;
    vk::UniqueFence fence;

    process_job(vk::Device dev, vk::CommandPool cmd_pool);

    void submit(vk::Queue queue);
    void wait_for_completion();
};

struct texture_process_job : public process_job {
    vk::ImageCreateInfo image_info;
    size_t total_size;
    std::unique_ptr<gpu_buffer> staging;
    std::unique_ptr<gpu_image> img;

    texture_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc, texture_info* info);

    void copy_to_dest(uint8_t* dest) const;

    void generate_mipmaps();
};

struct environment_process_job : public process_job {
    environment_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc);
};

class texture_processor {
    vk::UniqueInstance instance;
    vk::DebugReportCallbackEXT debug_report_callback;
    vk::PhysicalDevice phy_device;
    vk::UniqueDevice device;
    uint32_t graphics_queue_family_index;
    vk::Queue graphics_queue;

    vk::UniqueCommandPool cmd_pool;

    std::shared_ptr<gpu_allocator> allocator;

    std::unordered_map<texture_id, texture_process_job> jobs;
    std::unordered_map<string_id, environment_process_job> env_jobs;
public:
    texture_processor();
    ~texture_processor();

    void submit_texture(texture_id id, texture_info* info);
    void recieve_processed_texture(texture_id id, void* destination);

    environment_info submit_environment(string_id name, uint32_t width, uint32_t height, int nchannels, float* data);
};

