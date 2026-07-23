#pragma once
#ifdef USE_VULKAN
void vk_oit_lab_register( void );
/* Called at end of successful WBOIT resolve when fixtures may need evaluation. */
void vk_oit_lab_on_oit_resolved( void );
#endif
