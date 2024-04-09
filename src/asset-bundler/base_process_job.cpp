#include "asset-bundler/texture_process_jobs.h"
#include <error.h>

process_job::process_job(vk::Device dev, vk::CommandPool cmd_pool)
    : device(dev), total_output_size(0) {
    cmd_buffer = std::move(device.allocateCommandBuffersUnique(
        vk::CommandBufferAllocateInfo{cmd_pool, vk::CommandBufferLevel::ePrimary, 1}
    )[0]);

    fence = device.createFenceUnique(vk::FenceCreateInfo{});
}

void process_job::init_staging_buffer(const std::shared_ptr<gpu_allocator>& alloc, size_t size) {
    staging = std::make_unique<gpu_buffer>(
        alloc,
        vk::BufferCreateInfo{
            {}, size, vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst
        },
        VmaAllocationCreateInfo{
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                     | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        }
    );
}

void process_job::submit(vk::Queue queue) {
    cmd_buffer->end();
    queue.submit(vk::SubmitInfo{0, nullptr, nullptr, 1, &cmd_buffer.get()}, fence.get());
}

void process_job::wait_for_completion(uint8_t* result_dest) const {
    auto err = device.waitForFences(fence.get(), VK_TRUE, UINT64_MAX);
    if(err != vk::Result::eSuccess)
        throw vulkan_runtime_error("failed to run texture process job", err);

    auto* src = (uint8_t*)staging->cpu_mapped();
    // since we already directed the GPU to copy the image into the buffer in the right layout, we
    // just need to copy it out of GPU shared memory
    memcpy(result_dest, src, total_output_size);
}
