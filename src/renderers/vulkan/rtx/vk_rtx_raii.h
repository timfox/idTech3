/*
===============================================================================
RAII Vulkan Resource Management - C++23 Modern Implementation

Provides RAII wrappers for Vulkan resources to ensure proper cleanup
and exception-safe resource management
===============================================================================
*/

#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <functional>
#include <utility>
#include <concepts>
#include <type_traits>

// Forward declaration for VulkanResource base class
class VulkanResource;

// C++23 Concepts for Vulkan resource validation
template<typename T>
concept VulkanHandle = std::is_pointer_v<T> &&
                      (std::is_same_v<std::remove_pointer_t<T>, VkDevice_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkBuffer_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkImage_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkDeviceMemory_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkShaderModule_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkPipeline_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkDescriptorPool_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkCommandPool_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkSemaphore_T> ||
                       std::is_same_v<std::remove_pointer_t<T>, VkFence_T>);

template<typename T>
concept VulkanResourceWrapper = std::derived_from<T, VulkanResource>;

// Template metaprogramming utilities for Vulkan
namespace VulkanTraits {
    // Trait to get the Vulkan destroy function for a given handle type
    template<VulkanHandle T>
    struct DestroyFunction;

    template<> struct DestroyFunction<VkBuffer> {
        static constexpr auto function = vkDestroyBuffer;
    };

    template<> struct DestroyFunction<VkImage> {
        static constexpr auto function = vkDestroyImage;
    };

    template<> struct DestroyFunction<VkDeviceMemory> {
        static constexpr auto function = vkFreeMemory;
    };

    template<> struct DestroyFunction<VkShaderModule> {
        static constexpr auto function = vkDestroyShaderModule;
    };

    template<> struct DestroyFunction<VkPipeline> {
        static constexpr auto function = vkDestroyPipeline;
    };

    template<> struct DestroyFunction<VkDescriptorPool> {
        static constexpr auto function = vkDestroyDescriptorPool;
    };

    template<> struct DestroyFunction<VkCommandPool> {
        static constexpr auto function = vkDestroyCommandPool;
    };

    template<> struct DestroyFunction<VkSemaphore> {
        static constexpr auto function = vkDestroySemaphore;
    };

    template<> struct DestroyFunction<VkFence> {
        static constexpr auto function = vkDestroyFence;
    };
}

// Forward declarations
struct VkDevice_T;
typedef VkDevice_T* VkDevice;

// RAII base class for Vulkan resources
class VulkanResource {
protected:
    VkDevice device_{VK_NULL_HANDLE};

    VulkanResource(VkDevice device) noexcept : device_(device) {}
    virtual ~VulkanResource() = default;

public:
    VkDevice device() const noexcept { return device_; }
};

// RAII wrapper for VkBuffer and its memory
class VulkanBuffer : public VulkanResource {
private:
    VkBuffer buffer_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkDeviceSize size_{0};
    void* mapped_{nullptr};

public:
    // Constructor with buffer creation
    VulkanBuffer(VkDevice device,
                 VkPhysicalDevice physicalDevice,
                 VkDeviceSize size,
                 VkBufferUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 const std::function<void(VkBufferCreateInfo&)>& customizeCreateInfo = nullptr);

    // Move constructor
    VulkanBuffer(VulkanBuffer&& other) noexcept;

    // Destructor
    ~VulkanBuffer() override;

    // Delete copy operations
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(VulkanBuffer&&) = delete;

    // Accessors
    VkBuffer buffer() const noexcept { return buffer_; }
    VkDeviceMemory memory() const noexcept { return memory_; }
    VkDeviceSize size() const noexcept { return size_; }

    // Memory mapping utilities
    void* map(VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void unmap();
    bool isMapped() const noexcept { return mapped_ != nullptr; }

    // Data transfer utilities
    void copyFromHost(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);
    void copyToHost(void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    // Buffer info for descriptors
    VkDescriptorBufferInfo descriptorInfo(VkDeviceSize offset = 0,
                                        VkDeviceSize range = VK_WHOLE_SIZE) const noexcept;
};

// RAII wrapper for VkImage and its memory
class VulkanImage : public VulkanResource {
private:
    VkImage image_{VK_NULL_HANDLE};
    VkDeviceMemory memory_{VK_NULL_HANDLE};
    VkImageView view_{VK_NULL_HANDLE};
    VkExtent3D extent_{};
    VkFormat format_{VK_FORMAT_UNDEFINED};
    VkImageLayout currentLayout_{VK_IMAGE_LAYOUT_UNDEFINED};

public:
    // Constructor with image creation
    VulkanImage(VkDevice device,
                VkPhysicalDevice physicalDevice,
                VkExtent3D extent,
                VkFormat format,
                VkImageUsageFlags usage,
                VkMemoryPropertyFlags properties,
                VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                const std::function<void(VkImageCreateInfo&)>& customizeCreateInfo = nullptr);

    // Move constructor
    VulkanImage(VulkanImage&& other) noexcept;

    // Destructor
    ~VulkanImage() override;

    // Delete copy operations
    VulkanImage(const VulkanImage&) = delete;
    VulkanImage& operator=(const VulkanImage&) = delete;
    VulkanImage& operator=(VulkanImage&&) = delete;

    // Accessors
    VkImage image() const noexcept { return image_; }
    VkDeviceMemory memory() const noexcept { return memory_; }
    VkImageView view() const noexcept { return view_; }
    VkExtent3D extent() const noexcept { return extent_; }
    VkFormat format() const noexcept { return format_; }
    VkImageLayout currentLayout() const noexcept { return currentLayout_; }

    // Layout management
    void transitionLayout(VkCommandBuffer cmdBuffer,
                         VkImageLayout newLayout,
                         VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // Image info for descriptors
    VkDescriptorImageInfo descriptorInfo(VkSampler sampler = VK_NULL_HANDLE,
                                       VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const noexcept;
};

// RAII wrapper for VkShaderModule
class VulkanShaderModule : public VulkanResource {
private:
    VkShaderModule module_{VK_NULL_HANDLE};

public:
    // Constructor from SPIR-V code
    VulkanShaderModule(VkDevice device, const std::vector<uint32_t>& spirvCode);

    // Constructor from file (loads SPIR-V)
    VulkanShaderModule(VkDevice device, const char* filename);

    // Move constructor
    VulkanShaderModule(VulkanShaderModule&& other) noexcept;

    // Destructor
    ~VulkanShaderModule() override;

    // Delete copy operations
    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(VulkanShaderModule&&) = delete;

    // Accessors
    VkShaderModule module() const noexcept { return module_; }

    // Shader stage info for pipeline creation
    VkPipelineShaderStageCreateInfo stageInfo(VkShaderStageFlagBits stage,
                                            const char* entryPoint = "main") const noexcept;
};

// RAII wrapper for VkPipeline and VkPipelineLayout
class VulkanPipeline : public VulkanResource {
private:
    VkPipeline pipeline_{VK_NULL_HANDLE};
    VkPipelineLayout layout_{VK_NULL_HANDLE};
    VkPipelineBindPoint bindPoint_{VK_PIPELINE_BIND_POINT_GRAPHICS};

public:
    // Constructor for graphics pipeline
    VulkanPipeline(VkDevice device,
                   VkPipelineLayout layout,
                   const VkGraphicsPipelineCreateInfo& createInfo);

    // Constructor for compute pipeline
    VulkanPipeline(VkDevice device,
                   VkPipelineLayout layout,
                   const VkComputePipelineCreateInfo& createInfo);

    // Constructor for ray tracing pipeline
    VulkanPipeline(VkDevice device,
                   VkPipelineLayout layout,
                   const VkRayTracingPipelineCreateInfoNV& createInfo);

    // Move constructor
    VulkanPipeline(VulkanPipeline&& other) noexcept;

    // Destructor
    ~VulkanPipeline() override;

    // Delete copy operations
    VulkanPipeline(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline& operator=(VulkanPipeline&&) = delete;

    // Accessors
    VkPipeline pipeline() const noexcept { return pipeline_; }
    VkPipelineLayout layout() const noexcept { return layout_; }
    VkPipelineBindPoint bindPoint() const noexcept { return bindPoint_; }

    // Binding utilities
    void bind(VkCommandBuffer cmdBuffer) const noexcept;
    void bindDescriptorSets(VkCommandBuffer cmdBuffer,
                           uint32_t firstSet,
                           const std::vector<VkDescriptorSet>& descriptorSets,
                           const std::vector<uint32_t>& dynamicOffsets = {}) const;
};

// RAII wrapper for VkDescriptorPool and VkDescriptorSet
class VulkanDescriptorPool : public VulkanResource {
private:
    VkDescriptorPool pool_{VK_NULL_HANDLE};
    std::vector<VkDescriptorSet> descriptorSets_;
    VkDescriptorSetLayout layout_{VK_NULL_HANDLE};

public:
    // Constructor with pool creation
    VulkanDescriptorPool(VkDevice device,
                        const std::vector<VkDescriptorPoolSize>& poolSizes,
                        uint32_t maxSets,
                        const std::function<void(VkDescriptorPoolCreateInfo&)>& customizeCreateInfo = nullptr);

    // Move constructor
    VulkanDescriptorPool(VulkanDescriptorPool&& other) noexcept;

    // Destructor
    ~VulkanDescriptorPool() override;

    // Delete copy operations
    VulkanDescriptorPool(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(const VulkanDescriptorPool&) = delete;
    VulkanDescriptorPool& operator=(VulkanDescriptorPool&&) = delete;

    // Accessors
    VkDescriptorPool pool() const noexcept { return pool_; }
    const std::vector<VkDescriptorSet>& descriptorSets() const noexcept { return descriptorSets_; }

    // Descriptor set allocation
    std::vector<VkDescriptorSet> allocateSets(VkDescriptorSetLayout layout, uint32_t count);
    VkDescriptorSet allocateSet(VkDescriptorSetLayout layout);

    // Descriptor updates
    void updateSet(VkDescriptorSet set,
                   uint32_t binding,
                   const VkDescriptorBufferInfo& bufferInfo,
                   VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    void updateSet(VkDescriptorSet set,
                   uint32_t binding,
                   const VkDescriptorImageInfo& imageInfo,
                   VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    void updateSet(VkDescriptorSet set,
                   uint32_t binding,
                   const std::vector<VkDescriptorBufferInfo>& bufferInfos,
                   VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    void updateSet(VkDescriptorSet set,
                   uint32_t binding,
                   const std::vector<VkDescriptorImageInfo>& imageInfos,
                   VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
};

// RAII wrapper for command buffers
class VulkanCommandPool : public VulkanResource {
private:
    VkCommandPool pool_{VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> commandBuffers_;

public:
    // Constructor
    VulkanCommandPool(VkDevice device,
                     uint32_t queueFamilyIndex,
                     VkCommandPoolCreateFlags flags = 0);

    // Move constructor
    VulkanCommandPool(VulkanCommandPool&& other) noexcept;

    // Destructor
    ~VulkanCommandPool() override;

    // Delete copy operations
    VulkanCommandPool(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(const VulkanCommandPool&) = delete;
    VulkanCommandPool& operator=(VulkanCommandPool&&) = delete;

    // Accessors
    VkCommandPool pool() const noexcept { return pool_; }
    const std::vector<VkCommandBuffer>& commandBuffers() const noexcept { return commandBuffers_; }

    // Command buffer management
    VkCommandBuffer allocateBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
    std::vector<VkCommandBuffer> allocateBuffers(uint32_t count,
                                                VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

    void freeBuffer(VkCommandBuffer buffer);
    void freeBuffers(const std::vector<VkCommandBuffer>& buffers);

    // Utility functions
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkQueue queue);
};

// RAII wrapper for synchronization primitives
class VulkanSemaphore : public VulkanResource {
private:
    VkSemaphore semaphore_{VK_NULL_HANDLE};

public:
    // Constructor
    VulkanSemaphore(VkDevice device);

    // Move constructor
    VulkanSemaphore(VulkanSemaphore&& other) noexcept;

    // Destructor
    ~VulkanSemaphore() override;

    // Delete copy operations
    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(VulkanSemaphore&&) = delete;

    // Accessors
    VkSemaphore semaphore() const noexcept { return semaphore_; }
};

class VulkanFence : public VulkanResource {
private:
    VkFence fence_{VK_NULL_HANDLE};

public:
    // Constructor
    VulkanFence(VkDevice device, VkFenceCreateFlags flags = 0);

    // Move constructor
    VulkanFence(VulkanFence&& other) noexcept;

    // Destructor
    ~VulkanFence() override;

    // Delete copy operations
    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;
    VulkanFence& operator=(VulkanFence&&) = delete;

    // Accessors
    VkFence fence() const noexcept { return fence_; }

    // Fence operations
    void wait(uint64_t timeout = UINT64_MAX) const;
    void reset() const;
    VkResult getStatus() const noexcept;
};

// C++23 Template Metaprogramming for Vulkan Resources
namespace VulkanTemplates {

    // Template metaprogramming utilities for Vulkan
    template<VulkanHandle T>
    struct DestroyFunction;

    template<> struct DestroyFunction<VkBuffer> {
        static constexpr auto function = vkDestroyBuffer;
    };

    template<> struct DestroyFunction<VkImage> {
        static constexpr auto function = vkDestroyImage;
    };

    template<> struct DestroyFunction<VkDeviceMemory> {
        static constexpr auto function = vkFreeMemory;
    };

    template<> struct DestroyFunction<VkShaderModule> {
        static constexpr auto function = vkDestroyShaderModule;
    };

    template<> struct DestroyFunction<VkPipeline> {
        static constexpr auto function = vkDestroyPipeline;
    };

    template<> struct DestroyFunction<VkDescriptorPool> {
        static constexpr auto function = vkDestroyDescriptorPool;
    };

    template<> struct DestroyFunction<VkCommandPool> {
        static constexpr auto function = vkDestroyCommandPool;
    };

    template<> struct DestroyFunction<VkSemaphore> {
        static constexpr auto function = vkDestroySemaphore;
    };

    template<> struct DestroyFunction<VkFence> {
        static constexpr auto function = vkDestroyFence;
    };

    // Generic Vulkan resource wrapper using template metaprogramming
    template<VulkanHandle HandleType>
    class GenericVulkanResource : public VulkanResource {
    public:
        GenericVulkanResource(VkDevice device, HandleType handle = VK_NULL_HANDLE)
            : device_(device), handle_(handle) {}

        virtual ~GenericVulkanResource() = default;

        void destroy(VkDevice device) override {
            if (handle_ != VK_NULL_HANDLE) {
                DestroyFunction<HandleType>::function(device, handle_, nullptr);
                handle_ = VK_NULL_HANDLE;
            }
        }

        HandleType get() const noexcept { return handle_; }
        explicit operator bool() const noexcept { return handle_ != VK_NULL_HANDLE; }

    protected:
        VkDevice device_;
        HandleType handle_;
    };

    // Type aliases for common Vulkan resources using templates
    using VulkanBufferTemplate = GenericVulkanResource<VkBuffer>;
    using VulkanImageTemplate = GenericVulkanResource<VkImage>;
    using VulkanMemoryTemplate = GenericVulkanResource<VkDeviceMemory>;
    using VulkanShaderModuleTemplate = GenericVulkanResource<VkShaderModule>;
    using VulkanPipelineTemplate = GenericVulkanResource<VkPipeline>;
    using VulkanDescriptorPoolTemplate = GenericVulkanResource<VkDescriptorPool>;
    using VulkanCommandPoolTemplate = GenericVulkanResource<VkCommandPool>;
    using VulkanSemaphoreTemplate = GenericVulkanResource<VkSemaphore>;
    using VulkanFenceTemplate = GenericVulkanResource<VkFence>;

} // namespace VulkanTemplates

// Utility functions for common Vulkan operations
namespace VulkanUtils {
    // Memory type finding
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                           uint32_t typeFilter,
                           VkMemoryPropertyFlags properties);

    // Buffer creation helpers
    void createBuffer(VkDevice device,
                     VkDeviceSize size,
                     VkBufferUsageFlags usage,
                     VkBuffer& buffer);

    void allocateAndBindMemory(VkDevice device,
                              VkPhysicalDevice physicalDevice,
                              VkBuffer buffer,
                              VkMemoryPropertyFlags properties,
                              VkDeviceMemory& memory);

    // Image creation helpers
    void createImage(VkDevice device,
                    VkExtent3D extent,
                    VkFormat format,
                    VkImageUsageFlags usage,
                    VkImage& image);

    void createImageView(VkDevice device,
                        VkImage image,
                        VkFormat format,
                        VkImageAspectFlags aspectMask,
                        VkImageView& view);

    // Single-time command buffer execution
    void executeSingleTimeCommands(VkDevice device,
                                  VkCommandPool commandPool,
                                  VkQueue queue,
                                  const std::function<void(VkCommandBuffer)>& commands);

    // Resource copying utilities
    void copyBuffer(VkDevice device,
                   VkCommandPool commandPool,
                   VkQueue queue,
                   VkBuffer srcBuffer,
                   VkBuffer dstBuffer,
                   VkDeviceSize size);

    void copyBufferToImage(VkDevice device,
                          VkCommandPool commandPool,
                          VkQueue queue,
                          VkBuffer buffer,
                          VkImage image,
                          VkExtent3D extent);

    void transitionImageLayout(VkDevice device,
                              VkCommandPool commandPool,
                              VkQueue queue,
                              VkImage image,
                              VkFormat format,
                              VkImageLayout oldLayout,
                              VkImageLayout newLayout);
}