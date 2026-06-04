/*
===========================================================================
Vulkan device/instance function pointers (qvk*).
Split from vk.c for incremental modularization; assigned in vk_instance.c.
===========================================================================
*/

#include "tr_local.h"

#if defined( _DEBUG )
#define USE_VK_VALIDATION
#endif

PFN_vkCreateInstance								qvkCreateInstance;
PFN_vkEnumerateInstanceExtensionProperties		qvkEnumerateInstanceExtensionProperties;

PFN_vkCreateDevice								qvkCreateDevice;
PFN_vkDestroyInstance							qvkDestroyInstance;
PFN_vkEnumerateDeviceExtensionProperties			qvkEnumerateDeviceExtensionProperties;
PFN_vkEnumeratePhysicalDevices					qvkEnumeratePhysicalDevices;
PFN_vkGetDeviceProcAddr							qvkGetDeviceProcAddr;
PFN_vkGetPhysicalDeviceFeatures					qvkGetPhysicalDeviceFeatures;
PFN_vkGetPhysicalDeviceFeatures2					qvkGetPhysicalDeviceFeatures2;
PFN_vkGetPhysicalDeviceFormatProperties			qvkGetPhysicalDeviceFormatProperties;
PFN_vkGetPhysicalDeviceMemoryProperties			qvkGetPhysicalDeviceMemoryProperties;
PFN_vkGetPhysicalDeviceProperties				qvkGetPhysicalDeviceProperties;
PFN_vkGetPhysicalDeviceProperties2				qvkGetPhysicalDeviceProperties2;
PFN_vkGetPhysicalDeviceQueueFamilyProperties		qvkGetPhysicalDeviceQueueFamilyProperties;
PFN_vkDestroySurfaceKHR							qvkDestroySurfaceKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR			qvkGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR	qvkGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkGetPhysicalDeviceSurfaceSupportKHR			qvkGetPhysicalDeviceSurfaceSupportKHR;
#ifdef USE_VK_VALIDATION
PFN_vkCreateDebugReportCallbackEXT				qvkCreateDebugReportCallbackEXT;
PFN_vkDestroyDebugReportCallbackEXT				qvkDestroyDebugReportCallbackEXT;
#endif
PFN_vkAllocateCommandBuffers						qvkAllocateCommandBuffers;
PFN_vkAllocateDescriptorSets						qvkAllocateDescriptorSets;
PFN_vkAllocateMemory								qvkAllocateMemory;
PFN_vkBeginCommandBuffer							qvkBeginCommandBuffer;
PFN_vkBindBufferMemory							qvkBindBufferMemory;
PFN_vkBindImageMemory							qvkBindImageMemory;
PFN_vkCmdBeginRenderPass								qvkCmdBeginRenderPass;
PFN_vkCmdBindDescriptorSets						qvkCmdBindDescriptorSets;
PFN_vkCmdBindIndexBuffer							qvkCmdBindIndexBuffer;
PFN_vkCmdBindPipeline							qvkCmdBindPipeline;
PFN_vkCmdBindVertexBuffers						qvkCmdBindVertexBuffers;
PFN_vkCmdBlitImage								qvkCmdBlitImage;
PFN_vkCmdClearAttachments						qvkCmdClearAttachments;
PFN_vkCmdCopyBuffer								qvkCmdCopyBuffer;
PFN_vkCmdCopyBufferToImage						qvkCmdCopyBufferToImage;
PFN_vkCmdCopyImage								qvkCmdCopyImage;
PFN_vkCmdCopyImageToBuffer						qvkCmdCopyImageToBuffer;
PFN_vkCmdDraw									qvkCmdDraw;
PFN_vkCmdDrawIndexed								qvkCmdDrawIndexed;
PFN_vkCmdDispatch								qvkCmdDispatch;
PFN_vkCmdEndRenderPass									qvkCmdEndRenderPass;
PFN_vkCmdNextSubpass								qvkCmdNextSubpass;
PFN_vkCmdPipelineBarrier									qvkCmdPipelineBarrier;
PFN_vkCmdPushConstants									qvkCmdPushConstants;
PFN_vkCmdSetDepthBias							qvkCmdSetDepthBias;
PFN_vkCmdSetScissor										qvkCmdSetScissor;
PFN_vkCmdSetViewport										qvkCmdSetViewport;
PFN_vkCmdSetColorWriteMaskEXT						qvkCmdSetColorWriteMaskEXT;
PFN_vkCmdWriteTimestamp							qvkCmdWriteTimestamp;
PFN_vkCmdResetQueryPool							qvkCmdResetQueryPool;
PFN_vkCmdBeginQuery								qvkCmdBeginQuery;
PFN_vkCmdEndQuery								qvkCmdEndQuery;
PFN_vkCreateBuffer								qvkCreateBuffer;
PFN_vkCreateCommandPool							qvkCreateCommandPool;
PFN_vkCreateDescriptorPool						qvkCreateDescriptorPool;
PFN_vkCreateDescriptorSetLayout					qvkCreateDescriptorSetLayout;
PFN_vkCreateFence								qvkCreateFence;
PFN_vkCreateFramebuffer							qvkCreateFramebuffer;
PFN_vkCreateComputePipelines						qvkCreateComputePipelines;
PFN_vkCreateGraphicsPipelines					qvkCreateGraphicsPipelines;
PFN_vkCreateImage								qvkCreateImage;
PFN_vkCreateImageView							qvkCreateImageView;
PFN_vkCreatePipelineLayout						qvkCreatePipelineLayout;
PFN_vkCreatePipelineCache						qvkCreatePipelineCache;
PFN_vkCreateQueryPool							qvkCreateQueryPool;
PFN_vkCreateRenderPass							qvkCreateRenderPass;
PFN_vkCreateSampler								qvkCreateSampler;
PFN_vkCreateSemaphore							qvkCreateSemaphore;
PFN_vkCreateShaderModule							qvkCreateShaderModule;
PFN_vkDestroyBuffer								qvkDestroyBuffer;
PFN_vkDestroyCommandPool							qvkDestroyCommandPool;
PFN_vkDestroyDescriptorPool						qvkDestroyDescriptorPool;
PFN_vkDestroyDescriptorSetLayout					qvkDestroyDescriptorSetLayout;
PFN_vkDestroyDevice								qvkDestroyDevice;
PFN_vkDestroyFence								qvkDestroyFence;
PFN_vkDestroyFramebuffer							qvkDestroyFramebuffer;
PFN_vkDestroyImage								qvkDestroyImage;
PFN_vkDestroyImageView							qvkDestroyImageView;
PFN_vkDestroyPipeline							qvkDestroyPipeline;
PFN_vkDestroyPipelineCache						qvkDestroyPipelineCache;
PFN_vkDestroyPipelineLayout						qvkDestroyPipelineLayout;
PFN_vkDestroyQueryPool							qvkDestroyQueryPool;
PFN_vkDestroyRenderPass							qvkDestroyRenderPass;
PFN_vkDestroySampler								qvkDestroySampler;
PFN_vkDestroySemaphore							qvkDestroySemaphore;
PFN_vkDestroyShaderModule						qvkDestroyShaderModule;
PFN_vkDeviceWaitIdle								qvkDeviceWaitIdle;
PFN_vkEndCommandBuffer							qvkEndCommandBuffer;
PFN_vkFlushMappedMemoryRanges					qvkFlushMappedMemoryRanges;
PFN_vkFreeCommandBuffers							qvkFreeCommandBuffers;
PFN_vkFreeDescriptorSets							qvkFreeDescriptorSets;
PFN_vkFreeMemory									qvkFreeMemory;
PFN_vkGetBufferMemoryRequirements				qvkGetBufferMemoryRequirements;
PFN_vkGetDeviceQueue								qvkGetDeviceQueue;
PFN_vkGetImageMemoryRequirements					qvkGetImageMemoryRequirements;
PFN_vkGetImageSubresourceLayout					qvkGetImageSubresourceLayout;
PFN_vkGetPipelineCacheData						qvkGetPipelineCacheData;
PFN_vkInvalidateMappedMemoryRanges				qvkInvalidateMappedMemoryRanges;
PFN_vkMapMemory									qvkMapMemory;
PFN_vkQueueSubmit								qvkQueueSubmit;
PFN_vkQueueWaitIdle								qvkQueueWaitIdle;
PFN_vkResetCommandBuffer							qvkResetCommandBuffer;
PFN_vkResetDescriptorPool						qvkResetDescriptorPool;
PFN_vkResetFences								qvkResetFences;
PFN_vkGetQueryPoolResults						qvkGetQueryPoolResults;
PFN_vkResetQueryPoolEXT							qvkResetQueryPoolEXT;
PFN_vkUnmapMemory								qvkUnmapMemory;
PFN_vkUpdateDescriptorSets							qvkUpdateDescriptorSets;
PFN_vkWaitForFences								qvkWaitForFences;
PFN_vkAcquireNextImageKHR						qvkAcquireNextImageKHR;
PFN_vkCreateSwapchainKHR							qvkCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR						qvkDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR						qvkGetSwapchainImagesKHR;
PFN_vkQueuePresentKHR							qvkQueuePresentKHR;

PFN_vkGetBufferMemoryRequirements2KHR			qvkGetBufferMemoryRequirements2KHR;
PFN_vkGetImageMemoryRequirements2KHR				qvkGetImageMemoryRequirements2KHR;

PFN_vkDebugMarkerSetObjectNameEXT				qvkDebugMarkerSetObjectNameEXT;

PFN_vkCmdClearColorImage								qvkCmdClearColorImage;

#ifdef USE_VUDA
PFN_vkGetMemoryFdKHR									qvkGetMemoryFdKHR;
PFN_vkGetMemoryFdPropertiesKHR						qvkGetMemoryFdPropertiesKHR;
PFN_vkGetSemaphoreFdKHR								qvkGetSemaphoreFdKHR;
PFN_vkWaitSemaphoresKHR								qvkWaitSemaphoresKHR;
PFN_vkSignalSemaphoreKHR							qvkSignalSemaphoreKHR;
#endif

#ifdef USE_VULKAN_RTX
PFN_vkGetBufferDeviceAddress							qvkGetBufferDeviceAddress;
PFN_vkCreateAccelerationStructureKHR					qvkCreateAccelerationStructureKHR;
PFN_vkDestroyAccelerationStructureKHR					qvkDestroyAccelerationStructureKHR;
PFN_vkGetAccelerationStructureBuildSizesKHR			qvkGetAccelerationStructureBuildSizesKHR;
PFN_vkGetAccelerationStructureDeviceAddressKHR		qvkGetAccelerationStructureDeviceAddressKHR;
PFN_vkCmdBuildAccelerationStructuresKHR				qvkCmdBuildAccelerationStructuresKHR;
PFN_vkCreateRayTracingPipelinesKHR					qvkCreateRayTracingPipelinesKHR;
PFN_vkGetRayTracingShaderGroupHandlesKHR				qvkGetRayTracingShaderGroupHandlesKHR;
PFN_vkCmdTraceRaysKHR								qvkCmdTraceRaysKHR;
#endif
