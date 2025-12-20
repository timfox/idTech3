/*
=============================================================================
Vulkan Bindless Texture System Implementation

Bindless textures allow shaders to access textures through indices rather than
descriptor sets, providing better performance and flexibility.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_bindless.h"

#ifdef USE_VULKAN

// CVars
extern cvar_t *r_vk_bindlessTextures;

/*
=============================================================================
Bindless Texture Initialization
=============================================================================
*/

qboolean vk_bindless_init(void) {
	if (!r_vk_bindlessTextures || !r_vk_bindlessTextures->integer) {
		return qtrue; // Not enabled, but not an error
	}

	// Check if bindless textures are supported
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
	indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

	VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures2.pNext = &indexingFeatures;

	qvkGetPhysicalDeviceFeatures2KHR(vk.physical_device, &deviceFeatures2);

	if (!indexingFeatures.descriptorBindingPartiallyBound ||
		!indexingFeatures.runtimeDescriptorArray) {
		ri.Printf(PRINT_WARNING, "Vulkan: Bindless textures not supported by this device\n");
		return qfalse;
	}

	ri.Printf(PRINT_ALL, "Vulkan: Initializing bindless texture system\n");

	// Create bindless descriptor set layout
	VkDescriptorSetLayoutBinding binding = {};
	binding.binding = 0;
	binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding.descriptorCount = MAX_BINDLESS_TEXTURES;
	binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	binding.pImmutableSamplers = NULL;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = NULL;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &binding;

	// Set up descriptor indexing features
	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
	bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlags.bindingCount = 1;
	VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
									 VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;
	bindingFlags.pBindingFlags = &flags;

	layoutInfo.pNext = &bindingFlags;

	VK_CHECK(qvkCreateDescriptorSetLayout(vk.device, &layoutInfo, NULL, &vk.bindless_set_layout));

	// Create descriptor pool for bindless textures
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = MAX_BINDLESS_TEXTURES;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = 1;

	VK_CHECK(qvkCreateDescriptorPool(vk.device, &poolInfo, NULL, &vk.bindless_descriptor_pool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = vk.bindless_descriptor_pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vk.bindless_set_layout;

	VK_CHECK(qvkAllocateDescriptorSets(vk.device, &allocInfo, &vk.bindless_descriptor_set));

	// Create sampler for bindless textures
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = vk.maxAnisotropy;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;

	VK_CHECK(qvkCreateSampler(vk.device, &samplerInfo, NULL, &vk.bindless_sampler));

	vk.bindless_supported = qtrue;
	vk.bindless_texture_count = 0;

	ri.Printf(PRINT_ALL, "Vulkan: Bindless texture system initialized with capacity for %d textures\n", MAX_BINDLESS_TEXTURES);

	return qtrue;
}

/*
=============================================================================
Bindless Texture Management
=============================================================================
*/

uint32_t vk_bindless_register_texture(VkImageView imageView) {
	if (!vk.bindless_supported || vk.bindless_texture_count >= MAX_BINDLESS_TEXTURES) {
		return 0; // Invalid index
	}

	uint32_t index = vk.bindless_texture_count++;

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.sampler = vk.bindless_sampler;
	imageInfo.imageView = imageView;
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet write = {};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.bindless_descriptor_set;
	write.dstBinding = 0;
	write.dstArrayElement = index;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	qvkUpdateDescriptorSets(vk.device, 1, &write, 0, NULL);

	return index + 1; // Return 1-based index (0 = invalid)
}

void vk_bindless_shutdown(void) {
	if (!vk.bindless_supported) {
		return;
	}

	if (vk.bindless_sampler != VK_NULL_HANDLE) {
		qvkDestroySampler(vk.device, vk.bindless_sampler, NULL);
		vk.bindless_sampler = VK_NULL_HANDLE;
	}

	if (vk.bindless_descriptor_pool != VK_NULL_HANDLE) {
		qvkDestroyDescriptorPool(vk.device, vk.bindless_descriptor_pool, NULL);
		vk.bindless_descriptor_pool = VK_NULL_HANDLE;
	}

	if (vk.bindless_set_layout != VK_NULL_HANDLE) {
		qvkDestroyDescriptorSetLayout(vk.device, vk.bindless_set_layout, NULL);
		vk.bindless_set_layout = VK_NULL_HANDLE;
	}

	vk.bindless_supported = qfalse;
	vk.bindless_texture_count = 0;

	ri.Printf(PRINT_ALL, "Vulkan: Bindless texture system shut down\n");
}

#endif // USE_VULKAN
