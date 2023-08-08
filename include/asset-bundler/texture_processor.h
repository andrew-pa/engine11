#pragma once
#include "asset-bundler/model.h"
#include <vulkan/vulkan.hpp>
#include "egg/renderer/memory.h"

class texture_processor {
    vk::UniqueInstance instance;
    vk::DebugReportCallbackEXT debug_report_callback;
    vk::PhysicalDevice phy_device;
    vk::UniqueDevice device;
    uint32_t graphics_queue_family_index;
    vk::Queue graphics_queue;

    vk::UniqueCommandPool cmd_pool;

    std::shared_ptr<gpu_allocator> allocator;

    std::unordered_map<texture_id, struct texture_process_job> jobs;
    std::unordered_map<string_id, struct environment_process_job> env_jobs;
public:
    texture_processor();
    ~texture_processor();

    void submit_texture(texture_id id, texture_info* info);
    void recieve_processed_texture(texture_id id, void* destination);

    environment_info submit_environment(string_id name, uint32_t width, uint32_t height, int nchannels, float* data);
    void recieve_processed_environment(string_id name, void* destination);
};

