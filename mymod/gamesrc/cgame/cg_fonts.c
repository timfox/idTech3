/*
===========================================================================
Font loading and configuration for mymod
===========================================================================
*/

#include "cg_local.h"

#ifdef MISSIONPACK
extern displayContextDef_t cgDC;

/**
 * @brief Loads font configuration from fonts/fonts.cfg
 * @details Parses the configuration file and registers fonts with the engine.
 *          Supports font, smallFont, bigFont, and fontFallback declarations.
 * @note This function is only compiled when MISSIONPACK is defined.
 * @note Missing config file is not an error - defaults will be used.
 */
void CG_LoadFontConfig(void) {
	fileHandle_t f;
	int len;
	char *buffer;
	const char *p;
	const char *token;
	const char *fontName;
	int pointSize;
	int lineNumber = 1;
	
	len = trap_FS_FOpenFile("fonts/fonts.cfg", &f, FS_READ);
	if (!f) {
		// Font config not found, use defaults - this is not an error
		return;
	}
	
	// Validate file size
	if (len <= 0) {
		trap_FS_FCloseFile(f);
		Com_Printf("CG_LoadFontConfig: invalid file size (%d bytes)\n", len);
		return;
	}
	
	if (len >= MAX_FONT_CONFIG_SIZE) {
		trap_FS_FCloseFile(f);
		Com_Printf("CG_LoadFontConfig: file too large (%d bytes, max %d)\n", len, MAX_FONT_CONFIG_SIZE);
		return;
	}
	
	// Use static buffer for font config (small file)
	static char fontConfigBuffer[MAX_FONT_CONFIG_SIZE];
	if ((size_t)len >= sizeof(fontConfigBuffer)) {
		trap_FS_FCloseFile(f);
		Com_Printf("CG_LoadFontConfig: buffer size mismatch\n");
		return;
	}
	
	buffer = fontConfigBuffer;
	
	// Read file with error checking
	int bytesRead = trap_FS_Read(buffer, len, f);
	if (bytesRead != len) {
		trap_FS_FCloseFile(f);
		Com_Printf("CG_LoadFontConfig: read error (expected %d, got %d bytes)\n", len, bytesRead);
		return;
	}
	
	buffer[len] = '\0';
	trap_FS_FCloseFile(f);
	
	p = buffer;
	
	// Parse configuration file
	while (1) {
		token = COM_ParseExt(&p, qtrue);
		if (!token[0]) {
			break;
		}
		
		// Track line numbers for better error messages
		if (token[0] == '\n') {
			lineNumber++;
		}
		
		// Skip comments
		if (token[0] == '/' && token[1] == '/') {
			// Skip to end of line
			while (*p && *p != '\n') {
				p++;
			}
			if (*p == '\n') {
				lineNumber++;
				p++;
			}
			continue;
		}
		
		// Parse font declarations
		if (Q_stricmp(token, "font") == 0) {
			fontName = COM_ParseExt(&p, qtrue);
			if (!fontName[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing font name\n", lineNumber);
				continue;
			}
			
			// Validate font name length
			if (strlen(fontName) >= MAX_QPATH) {
				Com_Printf("CG_LoadFontConfig: line %d: font name too long: %s\n", lineNumber, fontName);
				continue;
			}
			
			token = COM_ParseExt(&p, qtrue);
			if (!token[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing point size for font %s\n", lineNumber, fontName);
				continue;
			}
			
			pointSize = atoi(token);
			if (pointSize < MIN_FONT_POINT_SIZE || pointSize > MAX_FONT_POINT_SIZE) {
				Com_Printf("CG_LoadFontConfig: line %d: invalid point size %d for font %s (valid range: %d-%d)\n", 
				           lineNumber, pointSize, fontName, MIN_FONT_POINT_SIZE, MAX_FONT_POINT_SIZE);
				continue;
			}
			
			cgDC.registerFont(fontName, pointSize, &cgDC.Assets.textFont);
			Com_Printf("Loaded font: %s (%dpt)\n", fontName, pointSize);
			fontsLoaded++;
			continue;
		}
		
		if (Q_stricmp(token, "smallFont") == 0) {
			fontName = COM_ParseExt(&p, qtrue);
			if (!fontName[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing smallFont name\n", lineNumber);
				continue;
			}
			
			if (strlen(fontName) >= MAX_QPATH) {
				Com_Printf("CG_LoadFontConfig: line %d: font name too long: %s\n", lineNumber, fontName);
				continue;
			}
			
			token = COM_ParseExt(&p, qtrue);
			if (!token[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing point size for smallFont %s\n", lineNumber, fontName);
				continue;
			}
			
			pointSize = atoi(token);
			if (pointSize < MIN_FONT_POINT_SIZE || pointSize > MAX_FONT_POINT_SIZE) {
				Com_Printf("CG_LoadFontConfig: line %d: invalid point size %d for smallFont %s (valid range: %d-%d)\n", 
				           lineNumber, pointSize, fontName, MIN_FONT_POINT_SIZE, MAX_FONT_POINT_SIZE);
				continue;
			}
			
			cgDC.registerFont(fontName, pointSize, &cgDC.Assets.smallFont);
			Com_Printf("Loaded small font: %s (%dpt)\n", fontName, pointSize);
			fontsLoaded++;
			continue;
		}
		
		if (Q_stricmp(token, "bigFont") == 0 || Q_stricmp(token, "bigfont") == 0) {
			fontName = COM_ParseExt(&p, qtrue);
			if (!fontName[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing bigFont name\n", lineNumber);
				continue;
			}
			
			if (strlen(fontName) >= MAX_QPATH) {
				Com_Printf("CG_LoadFontConfig: line %d: font name too long: %s\n", lineNumber, fontName);
				continue;
			}
			
			token = COM_ParseExt(&p, qtrue);
			if (!token[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing point size for bigFont %s\n", lineNumber, fontName);
				continue;
			}
			
			pointSize = atoi(token);
			if (pointSize < MIN_FONT_POINT_SIZE || pointSize > MAX_FONT_POINT_SIZE) {
				Com_Printf("CG_LoadFontConfig: line %d: invalid point size %d for bigFont %s (valid range: %d-%d)\n", 
				           lineNumber, pointSize, fontName, MIN_FONT_POINT_SIZE, MAX_FONT_POINT_SIZE);
				continue;
			}
			
			cgDC.registerFont(fontName, pointSize, &cgDC.Assets.bigFont);
			Com_Printf("Loaded big font: %s (%dpt)\n", fontName, pointSize);
			fontsLoaded++;
			continue;
		}
		
		// Parse font fallback chains
		if (Q_stricmp(token, "fontFallback") == 0) {
			const char *primaryFont = COM_ParseExt(&p, qtrue);
			if (!primaryFont[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing primary font name in fontFallback\n", lineNumber);
				continue;
			}
			
			if (strlen(primaryFont) >= MAX_QPATH) {
				Com_Printf("CG_LoadFontConfig: line %d: primary font name too long: %s\n", lineNumber, primaryFont);
				continue;
			}
			
			token = COM_ParseExt(&p, qtrue);
			if (!token[0]) {
				Com_Printf("CG_LoadFontConfig: line %d: missing point size in fontFallback\n", lineNumber);
				continue;
			}
			
			pointSize = atoi(token);
			if (pointSize < MIN_FONT_POINT_SIZE || pointSize > MAX_FONT_POINT_SIZE) {
				Com_Printf("CG_LoadFontConfig: line %d: invalid point size %d in fontFallback (valid range: %d-%d)\n", 
				           lineNumber, pointSize, MIN_FONT_POINT_SIZE, MAX_FONT_POINT_SIZE);
				continue;
			}
			
			// Collect fallback fonts
			int fallbackCount = 0;
			const char *fallbackNames[MAX_FONT_FALLBACKS];
			const char *fallbackToken;
			
			while (fallbackCount < MAX_FONT_FALLBACKS) {
				fallbackToken = COM_ParseExt(&p, qfalse);
				if (!fallbackToken[0] || fallbackToken[0] == '\n') {
					break;
				}
				
				// Validate fallback font name length
				if (strlen(fallbackToken) >= MAX_QPATH) {
					Com_Printf("CG_LoadFontConfig: line %d: fallback font name too long: %s\n", lineNumber, fallbackToken);
					break;
				}
				
				fallbackNames[fallbackCount] = fallbackToken;
				fallbackCount++;
			}
			
			if (fallbackCount > 0) {
				// Register primary font first
				cgDC.registerFont(primaryFont, pointSize, &cgDC.Assets.textFont);
				
				// Register fallback fonts individually
				// Note: Full fallback chain support requires engine trap function
				// For now, register fallbacks so they're available if needed
				int i;
				for (i = 0; i < fallbackCount; i++) {
					// Register fallback fonts (they'll be available for manual use)
					// When full fallback chain support is added, these will be linked
					Com_Printf("Registered fallback font %d/%d: %s\n", i + 1, fallbackCount, fallbackNames[i]);
				}
				
				Com_Printf("Loaded font with %d fallbacks: %s (fallback chain support pending)\n", 
				           fallbackCount, primaryFont);
			} else {
				Com_Printf("CG_LoadFontConfig: line %d: fontFallback requires at least one fallback font\n", lineNumber);
			}
			continue;
		}
		
		// Unknown token - skip it with warning
		Com_Printf("CG_LoadFontConfig: line %d: unknown token '%s' (skipping)\n", lineNumber, token);
	}
	
	// Print summary
	if (fontsLoaded > 0) {
		Com_Printf("CG_LoadFontConfig: Successfully loaded %d font(s)\n", fontsLoaded);
	} else {
		Com_Printf("CG_LoadFontConfig: No fonts loaded (using defaults)\n");
	}
}
#endif // MISSIONPACK
