#pragma once
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

// TODO: there is some way to make these exactly like std::unique_ptr, which would make them a lot
// more ergonomic while remaining safe

// an interface for VMA that makes sure everything happens in the main process even if called in a
// shared library
class gpu_allocator {
  public:
    virtual vk::Result create_buffer(
        const vk::BufferCreateInfo&    buffer_cfo,
        const VmaAllocationCreateInfo& alloc_cfo,
        VkBuffer*                      buf,
        VmaAllocation*                 alloc,
        VmaAllocationInfo*             alloc_info
    )                                                                = 0;
    virtual void destroy_buffer(vk::Buffer buf, VmaAllocation alloc) = 0;

    virtual vk::Result create_image(
        const vk::ImageCreateInfo&     image_cfo,
        const VmaAllocationCreateInfo& alloc_cfo,
        VkImage*                       img,
        VmaAllocation*                 alloc,
        VmaAllocationInfo*             alloc_info
    )                                                              = 0;
    virtual void destroy_image(vk::Image img, VmaAllocation alloc) = 0;

    virtual VmaAllocationInfo get_allocation_info(VmaAllocation alloc) = 0;

    virtual ~gpu_allocator() = default;
};

std::shared_ptr<gpu_allocator> create_allocator(const VmaAllocatorCreateInfo& cfo);

class gpu_buffer {
    std::shared_ptr<gpu_allocator> allocator;
    VkBuffer                       buf;
    VmaAllocation                  allocation;
    void*                          mapping;
    std::optional<std::string>     debug_name;

  public:
    gpu_buffer(
        std::shared_ptr<gpu_allocator> allocator,
        const vk::BufferCreateInfo&    buffer_cfo,
        const VmaAllocationCreateInfo& alloc_cfo
        = VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
    );
    ~gpu_buffer();

    gpu_buffer(const gpu_buffer&)            = delete;
    gpu_buffer& operator=(const gpu_buffer&) = delete;

    inline vk::Buffer get() { return buf; }

    void set_debug_name(vk::Instance inst, vk::Device dev, std::string&& debug_name);

    inline void* cpu_mapped() { return mapping; }
};

class gpu_image {
    std::shared_ptr<gpu_allocator> allocator;
    VkImage                        img;
    VmaAllocation                  allocation;
    std::optional<std::string>     debug_name;

  public:
    gpu_image(
        std::shared_ptr<gpu_allocator> allocator,
        const vk::ImageCreateInfo&     image_cfo,
        const VmaAllocationCreateInfo& alloc_cfo
        = VmaAllocationCreateInfo{.usage = VMA_MEMORY_USAGE_AUTO}
    );
    ~gpu_image();

    gpu_image(const gpu_image&)            = delete;
    gpu_image& operator=(const gpu_image&) = delete;

    inline vk::Image get() { return img; }

    void set_debug_name(vk::Instance inst, vk::Device dev, std::string&& debug_name);

    void* cpu_mapped() const;
};

std::vector<vk::BufferImageCopy> copy_regions_for_linear_image2d(
    uint32_t   width,
    uint32_t   height,
    uint32_t   mip_levels,
    uint32_t   array_layers,
    vk::Format format,
    size_t&    offset
);

template<typename T>
class gpu_shared_value : public gpu_buffer {
  public:
    gpu_shared_value(std::shared_ptr<gpu_allocator> a, vk::BufferUsageFlags usage)
        : gpu_buffer(
              a,
              vk::BufferCreateInfo{{}, sizeof(T), usage},
              VmaAllocationCreateInfo{
                  .flags
                  = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                  .usage = VMA_MEMORY_USAGE_AUTO
              }
          ) {}

    inline T* val() { return (T*)cpu_mapped(); }

    T* operator*() { return val(); }

    T* operator->() { return val(); }
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

    struct header {
        uint32_t max_index;
        uint32_t min_index;
        uint32_t padding[2];
    };

    size_t  header_offset, total_size;
    header* hdr;

  public:
    gpu_shared_value_heap(
        std::shared_ptr<gpu_allocator> allocator,
        size_t                         initial_max_size,
        vk::BufferUsageFlags           buffer_usage,
        bool                           include_header = false
    )
        : gpu_buffer(
              allocator,
              vk::BufferCreateInfo(
                  {
    },
                  initial_max_size * sizeof(T) + (include_header ? sizeof(header) : 0),
                  buffer_usage
              ),
              VmaAllocationCreateInfo{
                  .flags
                  = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
                  .usage = VMA_MEMORY_USAGE_AUTO
              }
          ),
          free_blocks{free_block{0, initial_max_size}},
          header_offset(include_header ? sizeof(header) : 0), hdr(nullptr),
          total_size(initial_max_size) {
        if(include_header) {
            hdr            = (header*)cpu_mapped();
            hdr->max_index = initial_max_size;
            hdr->min_index = initial_max_size;
        }
    }

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
        // std::cout << "alloc " << index << "\n";
        if(hdr != nullptr) {
            hdr->min_index = std::min(hdr->min_index, (uint32_t)index);
            hdr->max_index = std::max(hdr->max_index, (uint32_t)index);
        }
        return std::pair<T*, size_t>{((T*)((char*)cpu_mapped() + header_offset)) + index, index};
    }

    void free(size_t index) {
        if(hdr != nullptr) {
            hdr->min_index = std::max(hdr->min_index, (uint32_t)index);
            hdr->max_index = std::min(hdr->max_index, (uint32_t)index);
        }
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

    std::pair<size_t, size_t> current_valid_range() const {
        if(hdr != nullptr) return {hdr->min_index, hdr->max_index};
        return {0, total_size};
    }
};
