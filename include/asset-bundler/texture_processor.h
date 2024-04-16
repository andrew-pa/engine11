#pragma once
#include "asset-bundler/model.h"
#include "egg/renderer/memory.h"
#include "vk.h"

struct environment_process_job_resources {
    vk::UniquePipelineLayout      pipeline_layout;
    vk::UniqueDescriptorSetLayout desc_set_layout;
    vk::UniqueDescriptorPool      desc_pool;
    vk::UniquePipeline            skybox_pipeline, diffuse_map_pipeline;
    vk::UniqueSampler             sampler;

    environment_process_job_resources(vk::Device dev);
};

class texture_processor {
    vk::UniqueInstance         instance;
    vk::DebugReportCallbackEXT debug_report_callback;
    vk::PhysicalDevice         phy_device;
    vk::UniqueDevice           device;
    uint32_t                   graphics_queue_family_index;
    vk::Queue                  graphics_queue;

    vk::UniqueCommandPool cmd_pool;

    std::shared_ptr<gpu_allocator> allocator;

    std::unordered_map<texture_id, struct texture_process_job>    jobs;
    std::unordered_map<string_id, struct environment_process_job> env_jobs;

    std::unique_ptr<environment_process_job_resources> env_res;

    options opts;

  public:
    texture_processor(options opts);
    ~texture_processor();

    void submit_texture(texture_id id, texture_info* info);
    void recieve_processed_texture(texture_id id, void* destination);

    environment_info submit_environment(
        string_id name, uint32_t width, uint32_t height, int nchannels, float* data
    );
    void recieve_processed_environment(string_id name, void* destination);
};
