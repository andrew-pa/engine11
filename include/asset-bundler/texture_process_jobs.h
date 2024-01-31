#pragma once
#include "asset-bundler/model.h"
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>
#include "egg/renderer/memory.h"

struct process_job {
    vk::Device device;
    vk::UniqueCommandBuffer cmd_buffer;
    vk::UniqueFence fence;

    size_t total_output_size;
    std::unique_ptr<gpu_buffer> staging;

    process_job(vk::Device dev, vk::CommandPool cmd_pool);

    void submit(vk::Queue queue);
    void wait_for_completion(uint8_t* result_dest) const;
protected:
    void init_staging_buffer(const std::shared_ptr<gpu_allocator>& alloc, size_t size);
};

struct texture_process_job : public process_job {
    vk::ImageCreateInfo image_info;
    std::unique_ptr<gpu_image> img;

    texture_process_job(vk::Device dev, vk::CommandPool cmd_pool, const std::shared_ptr<gpu_allocator>& alloc, texture_info* info);

    void generate_mipmaps();
};

struct environment_process_job : public process_job {
    std::vector<vk::UniqueDescriptorSet> desc_sets;
    std::unique_ptr<gpu_image> src, skybox, diffuse_map;
    vk::UniqueImageView src_view, skybox_view, diffuse_map_view;


    environment_process_job(vk::Device dev, vk::CommandPool cmd_pool,
        struct environment_process_job_resources* res,
        const std::shared_ptr<gpu_allocator>& alloc, environment_info* info,
        uint32_t src_width, uint32_t src_height, int src_nchannels, float* src_data,
        bool enable_ibl_precomp);
private:

    vk::ImageCreateInfo src_image_info;
    vk::ImageCreateInfo sky_image_info;
    vk::ImageCreateInfo diffuse_map_image_info;

    void build_cmd_buffer(
        vk::CommandBuffer cmd_buffer,
        struct environment_process_job_resources* res
    );
};

size_t linear_image_size_in_bytes(const vk::ImageCreateInfo& image_info);
