#include "q_shared.h"
#include "qcommon.h"
#include "i18n.h"

static cvar_t *com_language;
static language_t *languages = NULL;
static language_t *currentLanguage = NULL;

/*
=================
I18n_FindLanguage
=================
Find a language by code
=================
*/
static language_t *I18n_FindLanguage(const char *code)
{
	language_t *lang;
	
	if (!code || !*code)
		return NULL;
	
	for (lang = languages; lang; lang = lang->next) {
		if (Q_stricmp(lang->code, code) == 0) {
			return lang;
		}
	}
	
	return NULL;
}

/*
=================
I18n_FindTranslation
=================
Find a translation entry in the current language
=================
*/
static translationEntry_t *I18n_FindTranslation(language_t *lang, const char *key)
{
	translationEntry_t *entry;
	
	if (!lang || !key || !*key)
		return NULL;
	
	for (entry = lang->entries; entry; entry = entry->next) {
		if (Q_stricmp(entry->key, key) == 0) {
			return entry;
		}
	}
	
	return NULL;
}

/*
=================
I18n_AddTranslation
=================
Add a translation entry to a language
=================
*/
static void I18n_AddTranslation(language_t *lang, const char *key, const char *text)
{
	translationEntry_t *entry;
	
	if (!lang || !key || !*key || !text)
		return;
	
	// Check if entry already exists
	entry = I18n_FindTranslation(lang, key);
	if (entry) {
		Q_strncpyz(entry->text, text, sizeof(entry->text));
		return;
	}
	
	// Create new entry
	entry = (translationEntry_t *)Z_Malloc(sizeof(translationEntry_t));
	if (!entry)
		return;
	
	Q_strncpyz(entry->key, key, sizeof(entry->key));
	Q_strncpyz(entry->text, text, sizeof(entry->text));
	entry->next = lang->entries;
	lang->entries = entry;
	lang->numEntries++;
}

/*
=================
I18n_CreateLanguage
=================
Create a new language entry
=================
*/
static language_t *I18n_CreateLanguage(const char *code, const char *name)
{
	language_t *lang;
	
	if (!code || !*code)
		return NULL;
	
	// Check if language already exists
	lang = I18n_FindLanguage(code);
	if (lang)
		return lang;
	
	// Create new language
	lang = (language_t *)Z_Malloc(sizeof(language_t));
	if (!lang)
		return NULL;
	
	Q_strncpyz(lang->code, code, sizeof(lang->code));
	if (name && *name) {
		Q_strncpyz(lang->name, name, sizeof(lang->name));
	} else {
		Q_strncpyz(lang->name, code, sizeof(lang->name));
	}
	
	lang->entries = NULL;
	lang->numEntries = 0;
	lang->next = languages;
	languages = lang;
	
	return lang;
}

/*
=================
I18n_ParseTranslationFile
=================
Parse a translation file (INI format)
Format: key=value
Lines starting with # or ; are comments
=================
*/
static qboolean I18n_ParseTranslationFile(const char *filename, language_t *lang)
{
	fileHandle_t f;
	int len;
	char *buffer;
	char *line;
	char *key, *value;
	char *newline;
	
	if (!filename || !lang)
		return qfalse;
	
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || !f)
		return qfalse;
	
	buffer = (char *)Z_Malloc(len + 1);
	if (!buffer) {
		FS_FCloseFile(f);
		return qfalse;
	}
	
	FS_Read(buffer, len, f);
	buffer[len] = '\0';
	FS_FCloseFile(f);
	
	line = buffer;
	while (*line) {
		// Skip whitespace
		while (*line == ' ' || *line == '\t') {
			line++;
		}
		
		// Skip empty lines and comments
		if (*line == '\0' || *line == '\n' || *line == '\r' ||
			*line == '#' || *line == ';') {
			// Find next line
			newline = strchr(line, '\n');
			if (newline) {
				line = newline + 1;
			} else {
				break;
			}
			continue;
		}
		
		// Find key
		key = line;
		value = strchr(line, '=');
		if (!value) {
			// No = found, skip line
			newline = strchr(line, '\n');
			if (newline) {
				line = newline + 1;
			} else {
				break;
			}
			continue;
		}
		
		*value++ = '\0';
		
		// Trim key
		while (*key == ' ' || *key == '\t') {
			key++;
		}
		{
			char *end = key + strlen(key) - 1;
			while (end > key && (*end == ' ' || *end == '\t')) {
				*end-- = '\0';
			}
		}
		
		// Trim value
		while (*value == ' ' || *value == '\t') {
			value++;
		}
		{
			char *end = value + strlen(value) - 1;
			while (end > value && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
				*end-- = '\0';
			}
		}
		
		// Add translation
		if (*key && *value) {
			I18n_AddTranslation(lang, key, value);
		}
		
		// Find next line
		newline = strchr(value, '\n');
		if (newline) {
			line = newline + 1;
		} else {
			break;
		}
	}
	
	Z_Free(buffer);
	return qtrue;
}

/*
=================
I18n_Init
=================
Initialize i18n system
=================
*/
void I18n_Init(void)
{
	com_language = Cvar_Get("com_language", LANGUAGE_ENGLISH, CVAR_ARCHIVE);
	Cvar_SetDescription(com_language, "Language code for translations (en, fr, de, es, it, ru, pl, cs)");
	
	languages = NULL;
	currentLanguage = NULL;
	
	// Create default English language
	currentLanguage = I18n_CreateLanguage(LANGUAGE_ENGLISH, "English");
	
	Com_Printf("I18n system initialized\n");
}

/*
=================
I18n_Shutdown
=================
Shutdown i18n system
=================
*/
void I18n_Shutdown(void)
{
	language_t *lang, *nextLang;
	translationEntry_t *entry, *nextEntry;
	
	for (lang = languages; lang; lang = nextLang) {
		nextLang = lang->next;
		
		for (entry = lang->entries; entry; entry = nextEntry) {
			nextEntry = entry->next;
			Z_Free(entry);
		}
		
		Z_Free(lang);
	}
	
	languages = NULL;
	currentLanguage = NULL;
}

/*
=================
I18n_LoadLanguageFile
=================
Load a translation file
=================
*/
qboolean I18n_LoadLanguageFile(const char *filename)
{
	char languageCode[MAX_LANGUAGE_CODE];
	char *dot;
	language_t *lang;
	
	if (!filename || !*filename)
		return qfalse;
	
	// Extract language code from filename (e.g., "translations/en.ini" -> "en")
	Q_strncpyz(languageCode, filename, sizeof(languageCode));
	dot = strrchr(languageCode, '/');
	if (dot) {
		Q_strncpyz(languageCode, dot + 1, sizeof(languageCode));
	}
	
	dot = strchr(languageCode, '.');
	if (dot) {
		*dot = '\0';
	}
	
	// Create or find language
	lang = I18n_FindLanguage(languageCode);
	if (!lang) {
		lang = I18n_CreateLanguage(languageCode, languageCode);
		if (!lang)
			return qfalse;
	}
	
	// Parse translation file
	return I18n_ParseTranslationFile(filename, lang);
}

/*
=================
I18n_LoadLanguage
=================
Load all translation files for a language
=================
*/
qboolean I18n_LoadLanguage(const char *languageCode)
{
	char filename[MAX_QPATH];
	char **fileList;
	int numFiles;
	int i;
	qboolean loaded = qfalse;
	
	if (!languageCode || !*languageCode)
		return qfalse;
	
	// Find all translation files for this language
	Com_sprintf(filename, sizeof(filename), "translations/%s", languageCode);
	fileList = FS_ListFiles(filename, ".ini", &numFiles);
	
	if (fileList && numFiles > 0) {
		for (i = 0; i < numFiles; i++) {
			if (!fileList[i])
				continue;
			
			Com_sprintf(filename, sizeof(filename), "translations/%s/%s", languageCode, fileList[i]);
			if (I18n_LoadLanguageFile(filename)) {
				loaded = qtrue;
			}
		}
		FS_FreeFileList(fileList);
	}
	
	// Also try single file format
	Com_sprintf(filename, sizeof(filename), "translations/%s.ini", languageCode);
	if (I18n_LoadLanguageFile(filename)) {
		loaded = qtrue;
	}
	
	return loaded;
}

/*
=================
I18n_Translate
=================
Translate a key to the current language
Returns the key itself if translation not found
=================
*/
const char *I18n_Translate(const char *key)
{
	translationEntry_t *entry;
	
	if (!key || !*key)
		return "";
	
	if (!currentLanguage) {
		return key;
	}
	
	entry = I18n_FindTranslation(currentLanguage, key);
	if (entry) {
		return entry->text;
	}
	
	// Fallback to English if not current language
	if (Q_stricmp(currentLanguage->code, LANGUAGE_ENGLISH) != 0) {
		language_t *english = I18n_FindLanguage(LANGUAGE_ENGLISH);
		if (english) {
			entry = I18n_FindTranslation(english, key);
			if (entry) {
				return entry->text;
			}
		}
	}
	
	// Return key as fallback
	return key;
}

/*
=================
I18n_TranslateFormat
=================
Translate a key and format with arguments
=================
*/
const char *I18n_TranslateFormat(const char *key, ...)
{
	static char buffer[MAX_TRANSLATION_TEXT];
	va_list argptr;
	const char *translated;
	
	if (!key || !*key)
		return "";
	
	translated = I18n_Translate(key);
	
	va_start(argptr, key);
	Q_vsnprintf(buffer, sizeof(buffer), translated, argptr);
	va_end(argptr);
	
	return buffer;
}

/*
=================
I18n_SetLanguage
=================
Set the current language
=================
*/
void I18n_SetLanguage(const char *languageCode)
{
	language_t *lang;
	
	if (!languageCode || !*languageCode)
		return;
	
	lang = I18n_FindLanguage(languageCode);
	if (!lang) {
		// Try to load the language
		if (!I18n_LoadLanguage(languageCode)) {
			Com_Printf("I18n_SetLanguage: Could not load language %s\n", languageCode);
			return;
		}
		lang = I18n_FindLanguage(languageCode);
		if (!lang)
			return;
	}
	
	currentLanguage = lang;
	if (com_language) {
		Cvar_Set("com_language", languageCode);
	}
	
	Com_Printf("Language set to: %s (%s)\n", lang->name, lang->code);
}

/*
=================
I18n_GetCurrentLanguage
=================
Get the current language code
=================
*/
const char *I18n_GetCurrentLanguage(void)
{
	if (currentLanguage) {
		return currentLanguage->code;
	}
	return LANGUAGE_ENGLISH;
}

/*
=================
I18n_LanguageExists
=================
Check if a language exists
=================
*/
qboolean I18n_LanguageExists(const char *languageCode)
{
	return I18n_FindLanguage(languageCode) != NULL;
}

/*
=================
I18n_GetLanguageCount
=================
Get the number of loaded languages
=================
*/
int I18n_GetLanguageCount(void)
{
	language_t *lang;
	int count = 0;
	
	for (lang = languages; lang; lang = lang->next) {
		count++;
	}
	
	return count;
}

/*
=================
I18n_ListLanguages
=================
List all available languages
=================
*/
void I18n_ListLanguages(void)
{
	language_t *lang;
	
	Com_Printf("Available languages:\n");
	for (lang = languages; lang; lang = lang->next) {
		Com_Printf("  %s - %s (%d translations)\n",
			lang->code, lang->name, lang->numEntries);
	}
}

