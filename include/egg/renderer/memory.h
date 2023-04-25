#pragma once
#include <iostream>
#include <list>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// TODO: there is some way to make these exactly like std::unique_ptr, which would make them a lot
// more ergonomic while remaining safe

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

// a heap of Ts shared between the CPU and GPU (~HOST_VISIBLE memory)
template<typename T>
class gpu_shared_value_heap : public gpu_buffer {
    struct free_block {
        free_block(size_t offset, size_t size) : offset(offset), size(size) {}

        size_t offset;
        size_t size;
    };

    std::list<free_block> free_blocks;

  public:
    gpu_shared_value_heap(
        VmaAllocator allocator, size_t initial_max_size, vk::BufferUsageFlags buffer_usage
    )
        : gpu_buffer(
            allocator,
            vk::BufferCreateInfo(
                {
    },
                initial_max_size * sizeof(T),
                buffer_usage
            ),
            VmaAllocationCreateInfo{
                .flags
                = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO}
        ),
          free_blocks{free_block{0, initial_max_size}} {}

    std::pair<T*, size_t> alloc() {
        if(free_blocks.empty()) {
            // TODO: we should reallocate and enlarge the heap instead here? we'll need a command
            // buffer to do the copy unless we do it on the CPU which is a little sad
            throw std::runtime_error("exhausted gpu_shared_value_heap");
        }
        auto& block = free_blocks.front();
        block.size--;
        size_t index = block.offset + block.size;
        if(block.size == 0) free_blocks.pop_front();
        std::cout << "alloc " << index << "\n";
        return std::pair<T*, size_t>{((T*)cpu_mapped()) + index, index};
    }

    void free(size_t index) {
        for(auto i = free_blocks.begin(); i != free_blocks.end(); ++i) {
            if(i->offset == index + 1) {
                i->offset--;
                i->size++;
                return;
            }
            if(i->offset + i->size == index) {
                i->size++;
                return;
            }
            if(i->offset > index) {
                free_blocks.insert(i, {index, 1});
                return;
            }
        }
        free_blocks.emplace_back(index, 1);
    }
};
