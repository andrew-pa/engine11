#pragma once
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

class gpu_buffer {
    VmaAllocator  allocator;
    VkBuffer      buf;
    VmaAllocation allocation;
    void*         mapping;

  public:
    gpu_buffer(
        VmaAllocator                   allocator,
        const vk::BufferCreateInfo&    buffer_cfo,
        const VmaAllocationCreateInfo& alloc_cfo
    );
    ~gpu_buffer();

    gpu_buffer(const gpu_buffer&)            = delete;
    gpu_buffer& operator=(const gpu_buffer&) = delete;

    inline vk::Buffer get() { return buf; }

    inline void* cpu_mapped() { return mapping; }
};

class gpu_image {
    VmaAllocator  allocator;
    VkImage       img;
    VmaAllocation allocation;

  public:
    gpu_image(
        VmaAllocator                   allocator,
        const vk::ImageCreateInfo&     image_cfo,
        const VmaAllocationCreateInfo& alloc_cfo
    );
    ~gpu_image();

    gpu_image(const gpu_image&)            = delete;
    gpu_image& operator=(const gpu_image&) = delete;

    inline vk::Image get() { return img; }
};