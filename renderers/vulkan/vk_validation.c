#include "tr_local.h"
#include "vk.h"
#include "vk_validation.h"
#include <stdio.h>

void vk_set_object_name( uint64_t obj, const char *objName, VkDebugReportObjectTypeEXT objType )
{
	if ( qvkDebugMarkerSetObjectNameEXT && obj )
	{
		VkDebugMarkerObjectNameInfoEXT info;
		info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		info.pNext = NULL;
		info.objectType = objType;
		info.object = obj;
		info.pObjectName = objName;
		qvkDebugMarkerSetObjectNameEXT( vk.device, &info );
	}
}

#ifdef USE_VK_VALIDATION
static qboolean vk_validation_error_pending = qfalse;
static char vk_validation_error_message[512];

static const char *vk_debug_report_severity( VkDebugReportFlagsEXT flags )
{
	if ( flags & VK_DEBUG_REPORT_ERROR_BIT_EXT ) {
		return "ERROR";
	}
	if ( flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT ) {
		return "PERFORMANCE WARNING";
	}
	if ( flags & VK_DEBUG_REPORT_WARNING_BIT_EXT ) {
		return "WARNING";
	}
	return "INFO";
}

static void vk_validation_record( VkDebugReportFlagsEXT flags, const char *message )
{
	const char *severity = vk_debug_report_severity( flags );
	const char *msg = message ? message : "<no message>";

	snprintf( vk_validation_error_message, sizeof( vk_validation_error_message ), "%s: %s", severity, msg );
	vk_validation_error_pending = qtrue;
}

VKAPI_ATTR VkBool32 VKAPI_CALL vk_validation_debug_callback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT object_type,
	uint64_t object,
	size_t location,
	int32_t message_code,
	const char *layer_prefix,
	const char *message,
	void *user_data )
{
	(void)object_type;
	(void)object;
	(void)location;
	(void)message_code;
	(void)layer_prefix;
	(void)user_data;

	if ( flags & (VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) ) {
		vk_validation_record( flags, message );
	}
#ifdef _WIN32
	MessageBoxA( 0, message, layer_prefix, MB_ICONWARNING );
	OutputDebugString( message );
	OutputDebugString( "\n" );
	DebugBreak();
#endif
	return VK_FALSE;
}

qboolean vk_consume_validation_error( char *buffer, size_t bufsize )
{
	if ( !vk_validation_error_pending ) {
		return qfalse;
	}

	Q_strncpyz( buffer, vk_validation_error_message, bufsize );
	vk_validation_error_pending = qfalse;
	return qtrue;
}
#else
VKAPI_ATTR VkBool32 VKAPI_CALL vk_validation_debug_callback(
	VkDebugReportFlagsEXT flags,
	VkDebugReportObjectTypeEXT object_type,
	uint64_t object,
	size_t location,
	int32_t message_code,
	const char *layer_prefix,
	const char *message,
	void *user_data )
{
	(void)flags;
	(void)object_type;
	(void)object;
	(void)location;
	(void)message_code;
	(void)layer_prefix;
	(void)message;
	(void)user_data;
	return VK_FALSE;
}

qboolean vk_consume_validation_error( char *buffer, size_t bufsize )
{
	(void)buffer;
	(void)bufsize;
	return qfalse;
}
#endif
