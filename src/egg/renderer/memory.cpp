#include "egg/renderer/memory.h"
#include "error.h"

gpu_buffer::gpu_buffer(
    std::shared_ptr<gpu_allocator> allocator,
    const vk::BufferCreateInfo&    buffer_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto res = allocator->create_buffer(buffer_cfo, alloc_cfo, &buf, &allocation, &info);
    if(res != vk::Result::eSuccess) throw vulkan_runtime_error("failed to allocate buffer", res);
    mapping = info.pMappedData;
}

gpu_buffer::~gpu_buffer() {
    allocator->destroy_buffer(buf, allocation);
    buf        = VK_NULL_HANDLE;
    allocation = nullptr;
}

void gpu_buffer::set_debug_name(vk::Instance inst, vk::Device dev, std::string&& debug_name) {
    this->debug_name = debug_name;
    dev.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            vk::ObjectType::eBuffer, (uint64_t)this->buf, this->debug_name.value().c_str()
        },
        vk::DispatchLoaderDynamic(inst, vkGetInstanceProcAddr)
    );
}

VkDeviceAddress gpu_buffer::device_address(vk::Device dev) const {
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = this->buf
    };
    return vkGetBufferDeviceAddress(dev, &info);
}

gpu_image::gpu_image(
    std::shared_ptr<gpu_allocator> allocator,
    const vk::ImageCreateInfo&     image_cfo,
    const VmaAllocationCreateInfo& alloc_cfo
)
    : allocator(allocator) {
    VmaAllocationInfo info;
    auto              res = allocator->create_image(image_cfo, alloc_cfo, &img, &allocation, &info);
    if(res != vk::Result::eSuccess) throw vulkan_runtime_error("failed to allocate image", res);
}

gpu_image::~gpu_image() {
    allocator->destroy_image(img, allocation);
    img        = VK_NULL_HANDLE;
    allocation = nullptr;
}

void* gpu_image::cpu_mapped() const {
    auto info = allocator->get_allocation_info(allocation);
    return info.pMappedData;
}

void gpu_image::set_debug_name(vk::Instance inst, vk::Device dev, std::string&& debug_name) {
    this->debug_name = debug_name;
    dev.setDebugUtilsObjectNameEXT(
        vk::DebugUtilsObjectNameInfoEXT{
            vk::ObjectType::eBuffer, (uint64_t)this->img, this->debug_name.value().c_str()
        },
        vk::DispatchLoaderDynamic(inst, vkGetInstanceProcAddr)
    );
}

#include <vulkan/vulkan_format_traits.hpp>

std::vector<vk::BufferImageCopy> copy_regions_for_linear_image2d(
    uint32_t   width,
    uint32_t   height,
    uint32_t   mip_levels,
    uint32_t   array_layers,
    vk::Format format,
    size_t&    offset
) {
    std::vector<vk::BufferImageCopy> regions;
    regions.reserve(mip_levels * array_layers);
    for(uint32_t layer_index = 0; layer_index < array_layers; ++layer_index) {
        uint32_t w = width, h = height;
        for(uint32_t mip_level = 0; mip_level < mip_levels; ++mip_level) {
            // std::cout << "L" << layer_index << " M" << mip_level << " " << std::hex <<
            // internal_offset << std::dec << "\n";
            regions.emplace_back(vk::BufferImageCopy{
                offset,
                0,
                0, // texels are tightly packed
                vk::ImageSubresourceLayers{
                                           vk::ImageAspectFlagBits::eColor, mip_level, layer_index, 1
                },
                vk::Offset3D{0, 0, 0},
                vk::Extent3D{w, h, 1},
            });
            offset += w * h * vk::blockSize(format);
            w = std::max(w / 2, 1u);
            h = std::max(h / 2, 1u);
        }
    }
    return regions;
}

struct vma_gpu_allocator : public gpu_allocator {
    VmaAllocator allocator;

    vma_gpu_allocator(const VmaAllocatorCreateInfo& cfo) {
        auto err = vmaCreateAllocator(&cfo, &allocator);
        if(err != VK_SUCCESS) throw vulkan_runtime_error("failed to create GPU allocator", err);
    }

    ~vma_gpu_allocator() override { vmaDestroyAllocator(allocator); }

    vk::Result create_buffer(
        const vk::BufferCreateInfo&    buffer_cfo,
        const VmaAllocationCreateInfo& alloc_cfo,
        VkBuffer*                      buf,
        VmaAllocation*                 alloc,
        VmaAllocationInfo*             alloc_info
    ) override {
        return vk::Result(vmaCreateBuffer(
            allocator, (VkBufferCreateInfo*)&buffer_cfo, &alloc_cfo, buf, alloc, alloc_info
        ));
    }

    void destroy_buffer(vk::Buffer buf, VmaAllocation alloc) override {
        vmaDestroyBuffer(allocator, buf, alloc);
    }

    vk::Result create_image(
        const vk::ImageCreateInfo&     image_cfo,
        const VmaAllocationCreateInfo& alloc_cfo,
        VkImage*                       img,
        VmaAllocation*                 alloc,
        VmaAllocationInfo*             alloc_info
    ) override {
        return vk::Result(vmaCreateImage(
            allocator, (VkImageCreateInfo*)&image_cfo, &alloc_cfo, img, alloc, alloc_info
        ));
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
