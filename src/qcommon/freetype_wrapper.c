/*
===========================================================================
This file provides FreeType integration for font rendering support.
It wraps FreeType functions with engine-style APIs.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"

#ifdef USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ERRORS_H
#include FT_SYSTEM_H
#include FT_IMAGE_H
#include FT_OUTLINE_H

// CVar to control FreeType usage
static cvar_t *com_freetype_enabled;

// Global FreeType library instance
static FT_Library ftLibrary = NULL;
static qboolean freetype_initialized = qfalse;

/*
=================
FreeType_Init
=================
Initialize FreeType subsystem
Returns qtrue on success, qfalse on failure
=================
*/
qboolean FreeType_Init(void)
{
	FT_Error error;

	if (freetype_initialized)
		return qtrue;

	com_freetype_enabled = Cvar_Get("com_freetype_enabled", "1", CVAR_ARCHIVE | CVAR_LATCH);
	Cvar_SetDescription(com_freetype_enabled, "Enable FreeType font rendering support (1 = enabled, 0 = disabled)");

	if (!com_freetype_enabled || !com_freetype_enabled->integer) {
		Com_Printf("FreeType support disabled.\n");
		return qfalse;
	}

	error = FT_Init_FreeType(&ftLibrary);
	if (error) {
		Com_Printf("FreeType_Init: Unable to initialize FreeType library (error %d)\n", error);
		return qfalse;
	}

	freetype_initialized = qtrue;
	Com_Printf("FreeType initialized successfully.\n");
	return qtrue;
}

/*
=================
FreeType_Shutdown
=================
Shutdown FreeType subsystem
=================
*/
void FreeType_Shutdown(void)
{
	if (!freetype_initialized)
		return;
	
	if (ftLibrary) {
		FT_Done_FreeType(ftLibrary);
		ftLibrary = NULL;
	}
	
	freetype_initialized = qfalse;
}

/*
=================
FreeType_GetLibrary
=================
Get the FreeType library instance
Returns NULL if not initialized
=================
*/
FT_Library FreeType_GetLibrary(void)
{
	if (!freetype_initialized || !com_freetype_enabled || !com_freetype_enabled->integer)
		return NULL;
	
	return ftLibrary;
}

/*
=================
FreeType_IsInitialized
=================
Check if FreeType is initialized
=================
*/
qboolean FreeType_IsInitialized(void)
{
	return freetype_initialized && ftLibrary != NULL;
}

/*
=================
FreeType_NewMemoryFace
=================
Create a new font face from memory
Returns FT_Err_Ok on success
=================
*/
FT_Error FreeType_NewMemoryFace(const FT_Byte *file_base, FT_Long file_size, FT_Long face_index, FT_Face *aface)
{
	if (!freetype_initialized || !ftLibrary)
		return FT_Err_Invalid_Library_Handle;
	
	if (!com_freetype_enabled || !com_freetype_enabled->integer)
		return FT_Err_Invalid_Library_Handle;
	
	return FT_New_Memory_Face(ftLibrary, file_base, file_size, face_index, aface);
}

/*
=================
FreeType_DoneFace
=================
Release a font face
=================
*/
void FreeType_DoneFace(FT_Face face)
{
	if (face) {
		FT_Done_Face(face);
	}
}

/*
=================
FreeType_SetCharSize
=================
Set character size for a face
Returns FT_Err_Ok on success
=================
*/
FT_Error FreeType_SetCharSize(FT_Face face, FT_F26Dot6 char_width, FT_F26Dot6 char_height, FT_UInt horz_resolution, FT_UInt vert_resolution)
{
	if (!face)
		return FT_Err_Invalid_Face_Handle;
	
	return FT_Set_Char_Size(face, char_width, char_height, horz_resolution, vert_resolution);
}

/*
=================
FreeType_LoadGlyph
=================
Load a glyph into a face's glyph slot
Returns FT_Err_Ok on success
=================
*/
FT_Error FreeType_LoadGlyph(FT_Face face, FT_UInt glyph_index, FT_Int32 load_flags)
{
	if (!face)
		return FT_Err_Invalid_Face_Handle;
	
	return FT_Load_Glyph(face, glyph_index, load_flags);
}

/*
=================
FreeType_GetCharIndex
=================
Get glyph index for a character code
Returns 0 if not found
=================
*/
FT_UInt FreeType_GetCharIndex(FT_Face face, FT_ULong charcode)
{
	if (!face)
		return 0;
	
	return FT_Get_Char_Index(face, charcode);
}

/*
=================
FreeType_RenderGlyph
=================
Render a glyph into a bitmap
Returns FT_Err_Ok on success
=================
*/
FT_Error FreeType_RenderGlyph(FT_GlyphSlot slot, int render_mode)
{
	if (!slot)
                return FT_Err_Invalid_Slot_Handle;
	
	return FT_Render_Glyph(slot, render_mode);
}

/*
=================
FreeType_OutlineTranslate
=================
Translate an outline
=================
*/
void FreeType_OutlineTranslate(FT_Outline *outline, FT_Pos xDelta, FT_Pos yDelta)
{
	if (outline) {
		FT_Outline_Translate(outline, xDelta, yDelta);
	}
}

/*
=================
FreeType_OutlineGetBitmap
=================
Convert an outline to a bitmap
Returns FT_Err_Ok on success
=================
*/
FT_Error FreeType_OutlineGetBitmap(FT_Library library, FT_Outline *outline, const FT_Bitmap *abitmap)
{
	if (!library || !outline || !abitmap)
		return FT_Err_Invalid_Argument;
	
	return FT_Outline_Get_Bitmap(library, outline, abitmap);
}

/*
=================
FreeType_GetKerning
=================
Get kerning between two glyphs
Returns kerning value in 26.6 fractional pixels
=================
*/
FT_Vector FreeType_GetKerning(FT_Face face, FT_UInt left_glyph, FT_UInt right_glyph, FT_UInt kern_mode)
{
	FT_Vector kerning = {0, 0};
	
	if (!face)
		return kerning;
	
	// Check if face has kerning
	if (FT_HAS_KERNING(face)) {
		FT_Error error = FT_Get_Kerning(face, left_glyph, right_glyph, kern_mode, &kerning);
		if (error) {
			kerning.x = 0;
			kerning.y = 0;
		}
	}
	
	return kerning;
}

/*
=================
FreeType_GetKerningDefault
=================
Get kerning using default mode (FT_KERNING_DEFAULT)
=================
*/
FT_Vector FreeType_GetKerningDefault(FT_Face face, FT_UInt left_glyph, FT_UInt right_glyph)
{
	return FreeType_GetKerning(face, left_glyph, right_glyph, FT_KERNING_DEFAULT);
}

/*
=================
FreeType_HasKerning
=================
Check if a font face supports kerning
=================
*/
qboolean FreeType_HasKerning(FT_Face face)
{
	if (!face)
		return qfalse;
	
	return FT_HAS_KERNING(face) ? qtrue : qfalse;
}

/*
=================
FreeType_GetCharWidth
=================
Get character advance width
Returns width in 26.6 fractional pixels
=================
*/
FT_Pos FreeType_GetCharWidth(FT_Face face, FT_UInt glyph_index)
{
	if (!face || !face->glyph)
		return 0;
	
	return face->glyph->metrics.horiAdvance;
}

/*
=================
FreeType_GetCharHeight
=================
Get character height metrics
Returns height in 26.6 fractional pixels
=================
*/
FT_Pos FreeType_GetCharHeight(FT_Face face, FT_UInt glyph_index)
{
	if (!face || !face->glyph)
		return 0;
	
	return face->glyph->metrics.height;
}

/*
=================
FreeType_GetFaceInfo
=================
Get font face information
=================
*/
qboolean FreeType_GetFaceInfo(FT_Face face, int *num_faces, int *face_index, char *family_name, int family_name_size, char *style_name, int style_name_size)
{
	if (!face)
		return qfalse;
	
	if (num_faces)
		*num_faces = face->num_faces;
	if (face_index)
		*face_index = face->face_index;
	if (family_name && face->family_name) {
		Q_strncpyz(family_name, face->family_name, family_name_size);
	}
	if (style_name && face->style_name) {
		Q_strncpyz(style_name, face->style_name, style_name_size);
	}
	
	return qtrue;
}

#endif // USE_FREETYPE

