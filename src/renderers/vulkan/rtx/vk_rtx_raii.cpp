/*
===============================================================================
RAII Vulkan Resource Management Implementation - C++23

Implementation of RAII wrappers for Vulkan resources providing automatic
cleanup and exception-safe resource management.
===============================================================================
*/

#include "vk_rtx_raii.h"
#include "../../common/qcommon.h" // For Com_Error, Com_Printf
#include <cstring> // For memcpy
#include <algorithm> // For std::min

//==============================================================================
// VulkanBuffer Implementation
//==============================================================================

VulkanBuffer::VulkanBuffer(VkDevice device,
                          VkPhysicalDevice physicalDevice,
                          VkDeviceSize size,
                          VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          const std::function<void(VkBufferCreateInfo&)>& customizeCreateInfo)
    : VulkanResource(device), size_(size)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (customizeCreateInfo) {
        customizeCreateInfo(bufferInfo);
    }

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create Vulkan buffer");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer_, nullptr);
        Com_Error(ERR_FATAL, "Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer_, memory_, 0);
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : VulkanResource(other.device_),
      buffer_(std::exchange(other.buffer_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
      size_(other.size_),
      mapped_(std::exchange(other.mapped_, nullptr))
{
}

VulkanBuffer::~VulkanBuffer()
{
    if (mapped_) {
        unmap();
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
    if (buffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, buffer_, nullptr);
    }
}

void* VulkanBuffer::map(VkDeviceSize offset, VkDeviceSize size)
{
    if (mapped_) {
        Com_Error(ERR_DROP, "Buffer is already mapped");
        return nullptr;
    }

    VkDeviceSize mapSize = (size == VK_WHOLE_SIZE) ? (size_ - offset) : size;
    if (vkMapMemory(device_, memory_, offset, mapSize, 0, &mapped_) != VK_SUCCESS) {
        Com_Error(ERR_DROP, "Failed to map buffer memory");
        return nullptr;
    }

    return mapped_;
}

void VulkanBuffer::unmap()
{
    if (!mapped_) {
        return;
    }

    vkUnmapMemory(device_, memory_);
    mapped_ = nullptr;
}

void VulkanBuffer::copyFromHost(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (!data) {
        Com_Error(ERR_DROP, "Invalid data pointer for buffer copy");
        return;
    }

    VkDeviceSize copySize = std::min(size, size_ - offset);
    void* mappedData = map(offset, copySize);
    if (mappedData) {
        memcpy(mappedData, data, copySize);
        unmap();
    }
}

void VulkanBuffer::copyToHost(void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (!data) {
        Com_Error(ERR_DROP, "Invalid data pointer for buffer copy");
        return;
    }

    VkDeviceSize copySize = std::min(size, size_ - offset);
    const void* mappedData = map(offset, copySize);
    if (mappedData) {
        memcpy(data, mappedData, copySize);
        unmap();
    }
}

VkDescriptorBufferInfo VulkanBuffer::descriptorInfo(VkDeviceSize offset, VkDeviceSize range) const noexcept
{
    VkDescriptorBufferInfo info{};
    info.buffer = buffer_;
    info.offset = offset;
    info.range = (range == VK_WHOLE_SIZE) ? size_ : range;
    return info;
}

//==============================================================================
// VulkanImage Implementation
//==============================================================================

VulkanImage::VulkanImage(VkDevice device,
                        VkPhysicalDevice physicalDevice,
                        VkExtent3D extent,
                        VkFormat format,
                        VkImageUsageFlags usage,
                        VkMemoryPropertyFlags properties,
                        VkImageAspectFlags aspectMask,
                        const std::function<void(VkImageCreateInfo&)>& customizeCreateInfo)
    : VulkanResource(device), extent_(extent), format_(format)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (customizeCreateInfo) {
        customizeCreateInfo(imageInfo);
    }

    if (vkCreateImage(device, &imageInfo, nullptr, &image_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create Vulkan image");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image_, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanUtils::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory_) != VK_SUCCESS) {
        vkDestroyImage(device, image_, nullptr);
        Com_Error(ERR_FATAL, "Failed to allocate image memory");
    }

    vkBindImageMemory(device, image_, memory_, 0);

    // Create image view
    VulkanUtils::createImageView(device, image_, format, aspectMask, view_);
}

VulkanImage::VulkanImage(VulkanImage&& other) noexcept
    : VulkanResource(other.device_),
      image_(std::exchange(other.image_, VK_NULL_HANDLE)),
      memory_(std::exchange(other.memory_, VK_NULL_HANDLE)),
      view_(std::exchange(other.view_, VK_NULL_HANDLE)),
      extent_(other.extent_),
      format_(other.format_),
      currentLayout_(other.currentLayout_)
{
}

VulkanImage::~VulkanImage()
{
    if (view_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, view_, nullptr);
    }
    if (memory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, memory_, nullptr);
    }
    if (image_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, image_, nullptr);
    }
}

void VulkanImage::transitionLayout(VkCommandBuffer cmdBuffer,
                                  VkImageLayout newLayout,
                                  VkPipelineStageFlags srcStage,
                                  VkPipelineStageFlags dstStage)
{
    if (currentLayout_ == newLayout) {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = currentLayout_;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image_;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    // Set appropriate access masks and pipeline stages based on layout transition
    switch (currentLayout_) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            barrier.srcAccessMask = 0;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        default:
            barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
    }

    switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            break;
        default:
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            break;
    }

    vkCmdPipelineBarrier(cmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    currentLayout_ = newLayout;
}

VkDescriptorImageInfo VulkanImage::descriptorInfo(VkSampler sampler, VkImageLayout layout) const noexcept
{
    VkDescriptorImageInfo info{};
    info.imageLayout = layout;
    info.imageView = view_;
    info.sampler = sampler;
    return info;
}

//==============================================================================
// VulkanShaderModule Implementation
//==============================================================================

VulkanShaderModule::VulkanShaderModule(VkDevice device, const std::vector<uint32_t>& spirvCode)
    : VulkanResource(device)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirvCode.size() * sizeof(uint32_t);
    createInfo.pCode = spirvCode.data();

    if (vkCreateShaderModule(device, &createInfo, nullptr, &module_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create shader module");
    }
}

VulkanShaderModule::VulkanShaderModule(VulkanShaderModule&& other) noexcept
    : VulkanResource(other.device_),
      module_(std::exchange(other.module_, VK_NULL_HANDLE))
{
}

VulkanShaderModule::~VulkanShaderModule()
{
    if (module_ != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device_, module_, nullptr);
    }
}

VkPipelineShaderStageCreateInfo VulkanShaderModule::stageInfo(VkShaderStageFlagBits stage,
                                                             const char* entryPoint) const noexcept
{
    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = module_;
    shaderStageInfo.pName = entryPoint;
    return shaderStageInfo;
}

//==============================================================================
// VulkanPipeline Implementation
//==============================================================================

VulkanPipeline::VulkanPipeline(VkDevice device,
                              VkPipelineLayout layout,
                              const VkGraphicsPipelineCreateInfo& createInfo)
    : VulkanResource(device), layout_(layout), bindPoint_(VK_PIPELINE_BIND_POINT_GRAPHICS)
{
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create graphics pipeline");
    }
}

VulkanPipeline::VulkanPipeline(VkDevice device,
                              VkPipelineLayout layout,
                              const VkComputePipelineCreateInfo& createInfo)
    : VulkanResource(device), layout_(layout), bindPoint_(VK_PIPELINE_BIND_POINT_COMPUTE)
{
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &createInfo, nullptr, &pipeline_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create compute pipeline");
    }
}

VulkanPipeline::VulkanPipeline(VkDevice device,
                              VkPipelineLayout layout,
                              const VkRayTracingPipelineCreateInfoNV& createInfo)
    : VulkanResource(device), layout_(layout), bindPoint_(VK_PIPELINE_BIND_POINT_RAY_TRACING_NV)
{
    (void)createInfo; // Suppress unused parameter warning
    // Note: Ray tracing pipeline creation would need the appropriate extension function
    // For now, we'll use a placeholder
    Com_Error(ERR_FATAL, "Ray tracing pipeline creation not implemented yet");
}

VulkanPipeline::VulkanPipeline(VulkanPipeline&& other) noexcept
    : VulkanResource(other.device_),
      pipeline_(std::exchange(other.pipeline_, VK_NULL_HANDLE)),
      layout_(std::exchange(other.layout_, VK_NULL_HANDLE)),
      bindPoint_(other.bindPoint_)
{
}

VulkanPipeline::~VulkanPipeline()
{
    if (pipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, pipeline_, nullptr);
    }
    // Note: We don't destroy the layout here as it might be shared
}

void VulkanPipeline::bind(VkCommandBuffer cmdBuffer) const noexcept
{
    vkCmdBindPipeline(cmdBuffer, bindPoint_, pipeline_);
}

void VulkanPipeline::bindDescriptorSets(VkCommandBuffer cmdBuffer,
                                       uint32_t firstSet,
                                       const std::vector<VkDescriptorSet>& descriptorSets,
                                       const std::vector<uint32_t>& dynamicOffsets) const
{
    vkCmdBindDescriptorSets(cmdBuffer, bindPoint_, layout_, firstSet,
                           static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(),
                           static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
}

//==============================================================================
// VulkanDescriptorPool Implementation
//==============================================================================

VulkanDescriptorPool::VulkanDescriptorPool(VkDevice device,
                                          const std::vector<VkDescriptorPoolSize>& poolSizes,
                                          uint32_t maxSets,
                                          const std::function<void(VkDescriptorPoolCreateInfo&)>& customizeCreateInfo)
    : VulkanResource(device)
{
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    if (customizeCreateInfo) {
        customizeCreateInfo(poolInfo);
    }

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create descriptor pool");
    }
}

VulkanDescriptorPool::VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept
    : VulkanResource(other.device_),
      pool_(std::exchange(other.pool_, VK_NULL_HANDLE)),
      descriptorSets_(std::move(other.descriptorSets_)),
      layout_(std::exchange(other.layout_, VK_NULL_HANDLE))
{
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
    // Free descriptor sets first
    if (!descriptorSets_.empty() && pool_ != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, pool_, static_cast<uint32_t>(descriptorSets_.size()),
                           descriptorSets_.data());
    }
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, pool_, nullptr);
    }
}

std::vector<VkDescriptorSet> VulkanDescriptorPool::allocateSets(VkDescriptorSetLayout layout, uint32_t count)
{
    std::vector<VkDescriptorSetLayout> layouts(count, layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool_;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> sets(count);
    if (vkAllocateDescriptorSets(device_, &allocInfo, sets.data()) != VK_SUCCESS) {
        Com_Error(ERR_DROP, "Failed to allocate descriptor sets");
        return {};
    }

    descriptorSets_.insert(descriptorSets_.end(), sets.begin(), sets.end());
    layout_ = layout;
    return sets;
}

VkDescriptorSet VulkanDescriptorPool::allocateSet(VkDescriptorSetLayout layout)
{
    auto sets = allocateSets(layout, 1);
    return sets.empty() ? VK_NULL_HANDLE : sets[0];
}

//==============================================================================
// VulkanCommandPool Implementation
//==============================================================================

VulkanCommandPool::VulkanCommandPool(VkDevice device,
                                    uint32_t queueFamilyIndex,
                                    VkCommandPoolCreateFlags flags)
    : VulkanResource(device)
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    poolInfo.flags = flags;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &pool_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create command pool");
    }
}

VulkanCommandPool::VulkanCommandPool(VulkanCommandPool&& other) noexcept
    : VulkanResource(other.device_),
      pool_(std::exchange(other.pool_, VK_NULL_HANDLE)),
      commandBuffers_(std::move(other.commandBuffers_))
{
}

VulkanCommandPool::~VulkanCommandPool()
{
    if (!commandBuffers_.empty() && pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, pool_, static_cast<uint32_t>(commandBuffers_.size()),
                           commandBuffers_.data());
    }
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, pool_, nullptr);
    }
}

VkCommandBuffer VulkanCommandPool::allocateBuffer(VkCommandBufferLevel level)
{
    auto buffers = allocateBuffers(1, level);
    return buffers.empty() ? VK_NULL_HANDLE : buffers[0];
}

std::vector<VkCommandBuffer> VulkanCommandPool::allocateBuffers(uint32_t count, VkCommandBufferLevel level)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool_;
    allocInfo.level = level;
    allocInfo.commandBufferCount = count;

    std::vector<VkCommandBuffer> buffers(count);
    if (vkAllocateCommandBuffers(device_, &allocInfo, buffers.data()) != VK_SUCCESS) {
        Com_Error(ERR_DROP, "Failed to allocate command buffers");
        return {};
    }

    commandBuffers_.insert(commandBuffers_.end(), buffers.begin(), buffers.end());
    return buffers;
}

void VulkanCommandPool::freeBuffer(VkCommandBuffer buffer)
{
    if (buffer != VK_NULL_HANDLE && pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, pool_, 1, &buffer);
        // Remove from our tracking list
        auto it = std::find(commandBuffers_.begin(), commandBuffers_.end(), buffer);
        if (it != commandBuffers_.end()) {
            commandBuffers_.erase(it);
        }
    }
}

void VulkanCommandPool::freeBuffers(const std::vector<VkCommandBuffer>& buffers)
{
    if (!buffers.empty() && pool_ != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device_, pool_, static_cast<uint32_t>(buffers.size()), buffers.data());
        // Remove from our tracking list
        for (auto buffer : buffers) {
            auto it = std::find(commandBuffers_.begin(), commandBuffers_.end(), buffer);
            if (it != commandBuffers_.end()) {
                commandBuffers_.erase(it);
            }
        }
    }
}

VkCommandBuffer VulkanCommandPool::beginSingleTimeCommands()
{
    VkCommandBuffer commandBuffer = allocateBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanCommandPool::endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue)
{
    vkEndCommandBuffer(commandBuffer);

    // Use fence for synchronization  instead of queue wait idle
    // This prevents premature device loss discovery and is more efficient
    static VkFence immediate_fence = VK_NULL_HANDLE;
    if (immediate_fence == VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = 0;
        if (vkCreateFence(device_, &fenceInfo, nullptr, &immediate_fence) != VK_SUCCESS) {
            // Fallback to queue wait idle if fence creation fails
            immediate_fence = VK_NULL_HANDLE;
        }
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, immediate_fence);
    if (submitResult != VK_SUCCESS) {
        // Handle device lost gracefully
        if (submitResult == VK_ERROR_DEVICE_LOST) {
            Com_Error(ERR_DROP, "Vulkan: Device lost during one-time command submit");
            return;
        }
        Com_Error(ERR_DROP, "Failed to submit one-time command buffer");
        return;
    }

    // Wait for this specific command using fence (better than queue wait idle)
    if (immediate_fence != VK_NULL_HANDLE) {
        VkResult fenceResult = vkWaitForFences(device_, 1, &immediate_fence, VK_TRUE, UINT64_MAX);
        if (fenceResult == VK_ERROR_DEVICE_LOST) {
            Com_Error(ERR_DROP, "Vulkan: Device lost during fence wait");
            return;
        } else if (fenceResult != VK_SUCCESS) {
            Com_Error(ERR_DROP, "Failed to wait for fence");
            return;
        }
        // Reset fence for next use
        vkResetFences(device_, 1, &immediate_fence);
    } else {
        // Fallback to queue wait idle if fence not available
        vkQueueWaitIdle(queue);
    }

    freeBuffer(commandBuffer);
}

//==============================================================================
// VulkanSemaphore Implementation
//==============================================================================

VulkanSemaphore::VulkanSemaphore(VkDevice device) : VulkanResource(device)
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &semaphore_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create semaphore");
    }
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept
    : VulkanResource(other.device_),
      semaphore_(std::exchange(other.semaphore_, VK_NULL_HANDLE))
{
}

VulkanSemaphore::~VulkanSemaphore()
{
    if (semaphore_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, semaphore_, nullptr);
    }
}

//==============================================================================
// VulkanFence Implementation
//==============================================================================

VulkanFence::VulkanFence(VkDevice device, VkFenceCreateFlags flags) : VulkanResource(device)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = flags;

    if (vkCreateFence(device, &fenceInfo, nullptr, &fence_) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create fence");
    }
}

VulkanFence::VulkanFence(VulkanFence&& other) noexcept
    : VulkanResource(other.device_),
      fence_(std::exchange(other.fence_, VK_NULL_HANDLE))
{
}

VulkanFence::~VulkanFence()
{
    if (fence_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, fence_, nullptr);
    }
}

void VulkanFence::wait(uint64_t timeout) const
{
    vkWaitForFences(device_, 1, &fence_, VK_TRUE, timeout);
}

void VulkanFence::reset() const
{
    vkResetFences(device_, 1, &fence_);
}

VkResult VulkanFence::getStatus() const noexcept
{
    return vkGetFenceStatus(device_, fence_);
}

//==============================================================================
// VulkanUtils Implementation
//==============================================================================

uint32_t VulkanUtils::findMemoryType(VkPhysicalDevice physicalDevice,
                                    uint32_t typeFilter,
                                    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    Com_Error(ERR_FATAL, "Failed to find suitable memory type");
    return 0; // Unreachable
}

void VulkanUtils::createBuffer(VkDevice device,
                              VkDeviceSize size,
                              VkBufferUsageFlags usage,
                              VkBuffer& buffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create buffer");
    }
}

void VulkanUtils::allocateAndBindMemory(VkDevice device,
                                       VkPhysicalDevice physicalDevice,
                                       VkBuffer buffer,
                                       VkMemoryPropertyFlags properties,
                                       VkDeviceMemory& memory)
{
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice,
                                              memRequirements.memoryTypeBits,
                                              properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to allocate buffer memory");
    }

    vkBindBufferMemory(device, buffer, memory, 0);
}

void VulkanUtils::createImage(VkDevice device,
                             VkExtent3D extent,
                             VkFormat format,
                             VkImageUsageFlags usage,
                             VkImage& image)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = extent;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create image");
    }
}

void VulkanUtils::createImageView(VkDevice device,
                                 VkImage image,
                                 VkFormat format,
                                 VkImageAspectFlags aspectMask,
                                 VkImageView& view)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "Failed to create image view");
    }
}

void VulkanUtils::executeSingleTimeCommands(VkDevice device,
                                           VkCommandPool commandPool,
                                           VkQueue queue,
                                           const std::function<void(VkCommandBuffer)>& commands)
{
    // Use fence for synchronization  instead of queue wait idle
    static VkFence immediate_fence = VK_NULL_HANDLE;
    if (immediate_fence == VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = 0;
        if (vkCreateFence(device, &fenceInfo, nullptr, &immediate_fence) != VK_SUCCESS) {
            // Fallback to queue wait idle if fence creation fails
            immediate_fence = VK_NULL_HANDLE;
        }
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    commands(commandBuffer);
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult submitResult = vkQueueSubmit(queue, 1, &submitInfo, immediate_fence);
    if (submitResult != VK_SUCCESS) {
        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
        if (submitResult == VK_ERROR_DEVICE_LOST) {
            Com_Error(ERR_DROP, "Vulkan: Device lost during one-time command submit");
            return;
        }
        Com_Error(ERR_DROP, "Failed to submit one-time command buffer");
        return;
    }

    // Wait for this specific command using fence (better than queue wait idle)
    if (immediate_fence != VK_NULL_HANDLE) {
        VkResult fenceResult = vkWaitForFences(device, 1, &immediate_fence, VK_TRUE, UINT64_MAX);
        if (fenceResult == VK_ERROR_DEVICE_LOST) {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            Com_Error(ERR_DROP, "Vulkan: Device lost during fence wait");
            return;
        } else if (fenceResult != VK_SUCCESS) {
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            Com_Error(ERR_DROP, "Failed to wait for fence");
            return;
        }
        // Reset fence for next use
        vkResetFences(device, 1, &immediate_fence);
    } else {
        // Fallback to queue wait idle if fence not available
        vkQueueWaitIdle(queue);
    }

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void VulkanUtils::copyBuffer(VkDevice device,
                            VkCommandPool commandPool,
                            VkQueue queue,
                            VkBuffer srcBuffer,
                            VkBuffer dstBuffer,
                            VkDeviceSize size)
{
    executeSingleTimeCommands(device, commandPool, queue,
        [&](VkCommandBuffer commandBuffer) {
            VkBufferCopy copyRegion{};
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
        });
}

void VulkanUtils::copyBufferToImage(VkDevice device,
                                   VkCommandPool commandPool,
                                   VkQueue queue,
                                   VkBuffer buffer,
                                   VkImage image,
                                   VkExtent3D extent)
{
    executeSingleTimeCommands(device, commandPool, queue,
        [&](VkCommandBuffer commandBuffer) {
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = {0, 0, 0};
            region.imageExtent = extent;

            vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        });
}

void VulkanUtils::transitionImageLayout(VkDevice device,
                                       VkCommandPool commandPool,
                                       VkQueue queue,
                                       VkImage image,
                                       VkFormat format,
                                       VkImageLayout oldLayout,
                                       VkImageLayout newLayout)
{
    (void)format; // Suppress unused parameter warning
    executeSingleTimeCommands(device, commandPool, queue,
        [&](VkCommandBuffer commandBuffer) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = oldLayout;
            barrier.newLayout = newLayout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags sourceStage;
            VkPipelineStageFlags destinationStage;

            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
                newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                       newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            } else {
                Com_Error(ERR_FATAL, "Unsupported layout transition");
            }

            vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage,
                               0, 0, nullptr, 0, nullptr, 1, &barrier);
        });
}