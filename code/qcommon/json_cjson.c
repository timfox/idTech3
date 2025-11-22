/*
===========================================================================
Copyright (C) 2024 id Tech 3

This file provides cJSON integration for enhanced JSON parsing capabilities.
It wraps cJSON functions with engine-style APIs and provides compatibility
with the existing json.h API when possible.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_CJSON
#include "cJSON.h"

// CVar to control JSON library usage
static cvar_t *com_json_library;

/*
=================
JSON_cJSON_Parse
=================
*/
cJSON *JSON_cJSON_Parse(const char *jsonString)
{
	if (!jsonString || !*jsonString)
		return NULL;
	
	return cJSON_Parse(jsonString);
}

/*
=================
JSON_cJSON_GetObjectItem
=================
*/
cJSON *JSON_cJSON_GetObjectItem(const cJSON *object, const char *string)
{
	if (!object || !string)
		return NULL;
	
	return cJSON_GetObjectItem(object, string);
}

/*
=================
JSON_cJSON_GetArrayItem
=================
*/
cJSON *JSON_cJSON_GetArrayItem(const cJSON *array, int item)
{
	if (!array)
		return NULL;
	
	return cJSON_GetArrayItem(array, item);
}

/*
=================
JSON_cJSON_GetArraySize
=================
*/
int JSON_cJSON_GetArraySize(const cJSON *array)
{
	if (!array)
		return 0;
	
	return cJSON_GetArraySize(array);
}

/*
=================
JSON_cJSON_GetStringValue
=================
*/
const char *JSON_cJSON_GetStringValue(const cJSON *item)
{
	if (!item)
		return NULL;
	
	return cJSON_GetStringValue(item);
}

/*
=================
JSON_cJSON_GetNumberValue
=================
*/
double JSON_cJSON_GetNumberValue(const cJSON *item)
{
	if (!item)
		return 0.0;
	
	return cJSON_GetNumberValue(item);
}

/*
=================
JSON_cJSON_IsString
=================
*/
qboolean JSON_cJSON_IsString(const cJSON *item)
{
	if (!item)
		return qfalse;
	
	return cJSON_IsString(item) ? qtrue : qfalse;
}

/*
=================
JSON_cJSON_IsNumber
=================
*/
qboolean JSON_cJSON_IsNumber(const cJSON *item)
{
	if (!item)
		return qfalse;
	
	return cJSON_IsNumber(item) ? qtrue : qfalse;
}

/*
=================
JSON_cJSON_IsObject
=================
*/
qboolean JSON_cJSON_IsObject(const cJSON *item)
{
	if (!item)
		return qfalse;
	
	return cJSON_IsObject(item) ? qtrue : qfalse;
}

/*
=================
JSON_cJSON_IsArray
=================
*/
qboolean JSON_cJSON_IsArray(const cJSON *item)
{
	if (!item)
		return qfalse;
	
	return cJSON_IsArray(item) ? qtrue : qfalse;
}

/*
=================
JSON_cJSON_Delete
=================
*/
void JSON_cJSON_Delete(cJSON *item)
{
	if (item)
		cJSON_Delete(item);
}

/*
=================
JSON_cJSON_Print
=================
*/
char *JSON_cJSON_Print(const cJSON *item)
{
	if (!item)
		return NULL;
	
	return cJSON_Print(item);
}

/*
=================
JSON_cJSON_PrintUnformatted
=================
*/
char *JSON_cJSON_PrintUnformatted(const cJSON *item)
{
	if (!item)
		return NULL;
	
	return cJSON_PrintUnformatted(item);
}

/*
=================
JSON_cJSON_CreateObject
=================
*/
cJSON *JSON_cJSON_CreateObject(void)
{
	return cJSON_CreateObject();
}

/*
=================
JSON_cJSON_CreateArray
=================
*/
cJSON *JSON_cJSON_CreateArray(void)
{
	return cJSON_CreateArray();
}

/*
=================
JSON_cJSON_CreateString
=================
*/
cJSON *JSON_cJSON_CreateString(const char *string)
{
	if (!string)
		return NULL;
	
	return cJSON_CreateString(string);
}

/*
=================
JSON_cJSON_CreateNumber
=================
*/
cJSON *JSON_cJSON_CreateNumber(double num)
{
	return cJSON_CreateNumber(num);
}

/*
=================
JSON_cJSON_AddItemToObject
=================
*/
void JSON_cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item)
{
	if (object && string && item)
		cJSON_AddItemToObject(object, string, item);
}

/*
=================
JSON_cJSON_AddItemToArray
=================
*/
void JSON_cJSON_AddItemToArray(cJSON *array, cJSON *item)
{
	if (array && item)
		cJSON_AddItemToArray(array, item);
}

/*
=================
JSON_Init
=================
Initialize JSON subsystem
=================
*/
void JSON_Init(void)
{
	com_json_library = Cvar_Get("com_json_library", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_json_library, "JSON library to use: 0 = old json.h, 1 = cJSON (default)");
}

#endif // USE_CJSON

