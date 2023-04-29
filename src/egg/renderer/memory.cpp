#include "egg/renderer/memory.h"
#include "error.h"

gpu_buffer::gpu_buffer(
    VmaAllocator                   allocator,
    const vk::BufferCreateInfo&    buffer_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto              res = vmaCreateBuffer(
        allocator, (VkBufferCreateInfo*)&buffer_cfo, &alloc_cfo, &buf, &allocation, &info
    );
    if(res != VK_SUCCESS) throw vulkan_runtime_error("failed to allocate buffer", res);
    mapping = info.pMappedData;
}

gpu_buffer::~gpu_buffer() {
    vmaDestroyBuffer(allocator, buf, allocation);
    buf        = VK_NULL_HANDLE;
    allocation = nullptr;
}

gpu_image::gpu_image(
    VmaAllocator                   allocator,
    const vk::ImageCreateInfo&     image_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto              res = vmaCreateImage(
        allocator, (VkImageCreateInfo*)&image_cfo, &alloc_cfo, &img, &allocation, &info
    );
    if(res != VK_SUCCESS) throw vulkan_runtime_error("failed to allocate image", res);
}

gpu_image::~gpu_image() {
    vmaDestroyImage(allocator, img, allocation);
    img        = VK_NULL_HANDLE;
    allocation = nullptr;
}
