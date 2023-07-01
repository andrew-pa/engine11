#include "egg/renderer/memory.h"
#include "error.h"

gpu_buffer::gpu_buffer(
    std::shared_ptr<gpu_allocator> allocator,
    const vk::BufferCreateInfo&    buffer_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto res = allocator->create_buffer(
        buffer_cfo, alloc_cfo, &buf, &allocation, &info
    );
    if(res != vk::Result::eSuccess) throw vulkan_runtime_error("failed to allocate buffer", res);
    mapping = info.pMappedData;
}

gpu_buffer::~gpu_buffer() {
    allocator->destroy_buffer(buf, allocation);
    buf        = VK_NULL_HANDLE;
    allocation = nullptr;
}

gpu_image::gpu_image(
    std::shared_ptr<gpu_allocator> allocator,
    const vk::ImageCreateInfo&     image_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto res = allocator->create_image(
        image_cfo, alloc_cfo, &img, &allocation, &info
    );
    if(res != vk::Result::eSuccess) throw vulkan_runtime_error("failed to allocate image", res);
}

void* gpu_image::cpu_mapped() const {
    auto info = allocator->get_allocation_info(allocation);
    return info.pMappedData;
}

gpu_image::~gpu_image() {
    allocator->destroy_image(img, allocation);
    img        = VK_NULL_HANDLE;
    allocation = nullptr;
}

struct vma_gpu_allocator : public gpu_allocator {
    VmaAllocator allocator;
    vma_gpu_allocator(const VmaAllocatorCreateInfo& cfo) {
		auto err = vmaCreateAllocator(&cfo, &allocator);
		if(err != VK_SUCCESS) throw vulkan_runtime_error("failed to create GPU allocator", err);
    }
    ~vma_gpu_allocator() override {
        vmaDestroyAllocator(allocator);
    }

     vk::Result create_buffer(const vk::BufferCreateInfo& buffer_cfo, const VmaAllocationCreateInfo& alloc_cfo, VkBuffer* buf, VmaAllocation* alloc, VmaAllocationInfo* alloc_info) override {
        return vk::Result(vmaCreateBuffer(allocator, (VkBufferCreateInfo*)&buffer_cfo, &alloc_cfo, buf, alloc, alloc_info));
    }

    void destroy_buffer(vk::Buffer buf, VmaAllocation alloc) override {
        vmaDestroyBuffer(allocator, buf, alloc);
    }

    vk::Result create_image(const vk::ImageCreateInfo& image_cfo, const VmaAllocationCreateInfo& alloc_cfo, VkImage* img, VmaAllocation* alloc, VmaAllocationInfo* alloc_info) override {
        return vk::Result(vmaCreateImage(allocator, (VkImageCreateInfo*)&image_cfo, &alloc_cfo, img, alloc, alloc_info));
    }

    void destroy_image(vk::Image img, VmaAllocation alloc) override {
        vmaDestroyImage(allocator, img, alloc);
    }

    VmaAllocationInfo get_allocation_info(VmaAllocation alloc) override {
        VmaAllocationInfo info;
        vmaGetAllocationInfo(allocator, alloc, &info);
        return info;
    }
};

std::shared_ptr<gpu_allocator> create_allocator(const VmaAllocatorCreateInfo& cfo) {
    return std::make_shared<vma_gpu_allocator>(cfo);
}
