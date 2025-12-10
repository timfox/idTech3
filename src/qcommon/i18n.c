#include "q_shared.h"
#include "qcommon.h"
#include "i18n.h"

#ifdef USE_CJSON
#include "cJSON.h"
#endif

#define LOC_HASH_SIZE          1024
#define LOC_DEBUG_RING         4
#define LOC_MISSING_LOG_PATH   "lang/missing_keys.txt"

typedef struct locEntry_s {
	char *key;
	char *value;
	struct locEntry_s *next;
} locEntry_t;

typedef struct locTable_s {
	char code[MAX_LANGUAGE_CODE];
	locEntry_t *buckets[LOC_HASH_SIZE];
	int numEntries;
} locTable_t;

static locTable_t loc_current;
static locTable_t loc_defaultLang;
static locTable_t loc_missingKeys;

static qboolean loc_initialized = qfalse;
static cvar_t *cl_language = NULL;
static cvar_t *cl_loc_debug = NULL;
static cvar_t *cl_loc_missingFile = NULL;
static cvar_t *cl_loc_trackMissing = NULL;
static cvar_t *com_language = NULL; // legacy alias for configs that still reference it

static char loc_activeCode[MAX_LANGUAGE_CODE];
static char loc_defaultCode[MAX_LANGUAGE_CODE] = DEFAULT_LANGUAGE_CODE;

static char loc_debugRing[LOC_DEBUG_RING][MAX_TRANSLATION_TEXT + MAX_TRANSLATION_KEY];
static int loc_debugRingIndex = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static unsigned int Loc_HashKey(const char *key)
{
	return MSG_HashKey(key, LOC_HASH_SIZE);
}

static void Loc_ClearTable(locTable_t *table)
{
	int i;

	if (!table) {
		return;
	}

	for (i = 0; i < LOC_HASH_SIZE; i++) {
		locEntry_t *entry = table->buckets[i];
		while (entry) {
			locEntry_t *next = entry->next;
			Z_Free(entry->key);
			Z_Free(entry->value);
			Z_Free(entry);
			entry = next;
		}
		table->buckets[i] = NULL;
	}

	table->numEntries = 0;
	table->code[0] = '\0';
}

static locEntry_t *Loc_Find(const locTable_t *table, const char *key)
{
	locEntry_t *entry;

	if (!table || !key || !*key) {
		return NULL;
	}

	for (entry = table->buckets[Loc_HashKey(key)]; entry; entry = entry->next) {
		if (Q_stricmp(entry->key, key) == 0) {
			return entry;
		}
	}

	return NULL;
}

static qboolean Loc_Insert(locTable_t *table, const char *key, const char *value)
{
	locEntry_t *entry;
	unsigned int hash;

	if (!table || !key || !*key || !value) {
		return qfalse;
	}

	hash = Loc_HashKey(key);
	for (entry = table->buckets[hash]; entry; entry = entry->next) {
		if (Q_stricmp(entry->key, key) == 0) {
			Z_Free(entry->value);
			entry->value = CopyString(value);
			return qtrue;
		}
	}

	entry = (locEntry_t *)Z_Malloc(sizeof(locEntry_t));
	if (!entry) {
		return qfalse;
	}

	entry->key = CopyString(key);
	entry->value = CopyString(value);
	entry->next = table->buckets[hash];
	table->buckets[hash] = entry;
	table->numEntries++;
	return qtrue;
}

static void Loc_NormalizeLanguageCode(const char *in, char *out, size_t outSize)
{
	if (!out || outSize == 0) {
		return;
	}

	if (!in || !*in) {
		Q_strncpyz(out, loc_defaultCode, outSize);
	} else {
		Q_strncpyz(out, in, outSize);
		Q_strlwr(out);
	}
}

static const char *Loc_DebugWrap(const char *id, const char *value)
{
	if (!cl_loc_debug || cl_loc_debug->integer == 0) {
		return value;
	}

	if (cl_loc_debug->integer >= 2) {
		return id ? id : "";
	}

	// Wrap localized text to make it obvious when localization is active
	{
		int slot = loc_debugRingIndex++ % LOC_DEBUG_RING;
		Com_sprintf(loc_debugRing[slot], sizeof(loc_debugRing[slot]), "[L]%s[/L]", value ? value : "");
		return loc_debugRing[slot];
	}
}

static qboolean Loc_AddMissingKey(const char *key)
{
	if (!key || !*key) {
		return qfalse;
	}

	if (Loc_Find(&loc_missingKeys, key)) {
		return qfalse; // already tracked
	}

	return Loc_Insert(&loc_missingKeys, key, key);
}

static void Loc_LogMissing(const char *key)
{
	fileHandle_t f;

	if (!cl_loc_trackMissing || !cl_loc_trackMissing->integer) {
		return;
	}

	if (!Loc_AddMissingKey(key)) {
		return;
	}

	Com_Printf(S_COLOR_YELLOW "Missing localization key: %s\n", key);

	if (!cl_loc_missingFile || !cl_loc_missingFile->string[0]) {
		return;
	}

	f = FS_FOpenFileAppend(cl_loc_missingFile->string);
	if (f == FS_INVALID_HANDLE) {
		return;
	}

	FS_Printf(f, "%s\n", key);
	FS_FCloseFile(f);
}

// ---------------------------------------------------------------------------
// File loading
// ---------------------------------------------------------------------------

static qboolean Loc_LoadJsonFile(const char *filename, locTable_t *table)
{
	fileHandle_t f;
	int len;
	char *buffer;
	qboolean loaded = qfalse;

	if (!filename || !table) {
		return qfalse;
	}

	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || f == FS_INVALID_HANDLE) {
		return qfalse;
	}

	buffer = (char *)Z_Malloc(len + 1);
	if (!buffer) {
		FS_FCloseFile(f);
		return qfalse;
	}

	FS_Read(buffer, len, f);
	buffer[len] = '\0';
	FS_FCloseFile(f);

#ifdef USE_CJSON
	{
		cJSON *root = cJSON_Parse(buffer);
		cJSON *child;

		Z_Free(buffer);

		if (!root) {
			return qfalse;
		}

		if (!cJSON_IsObject(root)) {
			cJSON_Delete(root);
			return qfalse;
		}

		for (child = root->child; child; child = child->next) {
			if (!child->string) {
				continue;
			}
			if (cJSON_IsString(child) && child->valuestring) {
				if (Loc_Insert(table, child->string, child->valuestring)) {
					loaded = qtrue;
				}
			}
		}

		cJSON_Delete(root);
	}
#else
	Z_Free(buffer);
	Com_Printf("Localization: JSON support disabled, cannot load %s\n", filename);
#endif

	return loaded;
}

// Legacy INI format loader for compatibility
static qboolean Loc_ParseLegacyIni(const char *filename, locTable_t *table)
{
	fileHandle_t f;
	int len;
	char *buffer;
	char *line;
	char *key, *value;
	char *newline;
	qboolean loaded = qfalse;

	if (!filename || !table) {
		return qfalse;
	}

	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || f == FS_INVALID_HANDLE) {
		return qfalse;
	}

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
		while (*line == ' ' || *line == '\t') {
			line++;
		}

		if (*line == '\0' || *line == '\n' || *line == '\r' || *line == '#' || *line == ';') {
			newline = strchr(line, '\n');
			if (newline) {
				line = newline + 1;
			} else {
				break;
			}
			continue;
		}

		key = line;
		value = strchr(line, '=');
		if (!value) {
			newline = strchr(line, '\n');
			if (newline) {
				line = newline + 1;
			} else {
				break;
			}
			continue;
		}

		*value++ = '\0';

		while (*key == ' ' || *key == '\t') {
			key++;
		}
		{
			char *end = key + strlen(key) - 1;
			while (end > key && (*end == ' ' || *end == '\t')) {
				*end-- = '\0';
			}
		}

		while (*value == ' ' || *value == '\t') {
			value++;
		}
		{
			char *end = value + strlen(value) - 1;
			while (end > value && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
				*end-- = '\0';
			}
		}

		if (*key && *value) {
			if (Loc_Insert(table, key, value)) {
				loaded = qtrue;
			}
		}

		newline = strchr(value, '\n');
		if (newline) {
			line = newline + 1;
		} else {
			break;
		}
	}

	Z_Free(buffer);
	return loaded;
}

static qboolean Loc_LoadIntoTable(const char *languageCode, locTable_t *table)
{
	char filename[MAX_QPATH];
	qboolean loaded = qfalse;

	if (!table) {
		return qfalse;
	}

	Loc_ClearTable(table);
	Q_strncpyz(table->code, languageCode, sizeof(table->code));

	// Preferred: JSON file under lang/
	Com_sprintf(filename, sizeof(filename), "lang/lang_%s.json", languageCode);
	loaded = Loc_LoadJsonFile(filename, table);

	// Legacy fallback: single INI file
	if (!loaded) {
		Com_sprintf(filename, sizeof(filename), "translations/%s.ini", languageCode);
		loaded = Loc_ParseLegacyIni(filename, table);
	}

	// Legacy fallback: lang/lang_xx.ini
	if (!loaded) {
		Com_sprintf(filename, sizeof(filename), "lang/lang_%s.ini", languageCode);
		loaded = Loc_ParseLegacyIni(filename, table);
	}

	return loaded;
}

static qboolean Loc_LoadLanguageTables(const char *languageCode)
{
	char normalized[MAX_LANGUAGE_CODE];
	qboolean loaded = qfalse;

	Loc_NormalizeLanguageCode(languageCode, normalized, sizeof(normalized));

	// Always refresh default language so fallback text stays up to date
	Loc_LoadIntoTable(loc_defaultCode, &loc_defaultLang);

	loaded = Loc_LoadIntoTable(normalized, &loc_current);
	if (!loaded && Q_stricmp(normalized, loc_defaultCode) != 0) {
		Com_Printf("Localization: unable to load '%s', falling back to '%s'\n", normalized, loc_defaultCode);
		Loc_LoadIntoTable(loc_defaultCode, &loc_current);
	}

	Q_strncpyz(loc_activeCode,
		loc_current.code[0] ? loc_current.code : loc_defaultCode,
		sizeof(loc_activeCode));

	// Reset missing-key tracking for the new language
	Loc_ClearTable(&loc_missingKeys);

	if (com_language) {
		Cvar_Set("com_language", loc_activeCode);
	}

	return loaded;
}

static void CL_Localize_Reload_f(void)
{
	const char *target = (cl_language && cl_language->string[0]) ? cl_language->string : loc_activeCode;

	Loc_LoadLanguageTables(target);
	Com_Printf("Localization reloaded for '%s'\n", loc_activeCode);
}

static void CL_Localize_Test_f(void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: loc_test <stringId>\n");
		return;
	}

	Com_Printf("%s\n", CL_Localize(Cmd_Argv(1)));
}

static void CL_Localize_TestReplace_f(void)
{
	locVar_t vars[8];
	int varCount = 0;
	int argc = Cmd_Argc();
	int i;

	if (argc < 2) {
		Com_Printf("Usage: loc_test_replace <stringId> <key> <value> [key value]...\n");
		return;
	}

	// pairs start at arg 2
	for (i = 2; i + 1 < argc && varCount < (int)ARRAY_LEN(vars); i += 2) {
		vars[varCount].key = Cmd_Argv(i);
		vars[varCount].value = Cmd_Argv(i + 1);
		varCount++;
	}

	Com_Printf("%s\n", CL_LocalizeReplace(Cmd_Argv(1), vars, varCount));
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void CL_Localize_Init(void)
{
	if (loc_initialized) {
		return;
	}

	cl_language = Cvar_Get("cl_language", DEFAULT_LANGUAGE_CODE, CVAR_ARCHIVE);
	Cvar_SetDescription(cl_language, "Active language code (\"en\", \"fr\", \"pl\"...)");

	// Legacy alias preserved for compatibility with existing configs
	com_language = Cvar_Get("com_language", cl_language->string, CVAR_ARCHIVE);
	Cvar_SetDescription(com_language, "Deprecated: use cl_language instead");

	cl_loc_debug = Cvar_Get("cl_loc_debug", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_loc_debug, "Localization debug mode (0=off, 1=wrap text, 2=show keys)");

	cl_loc_missingFile = Cvar_Get("cl_loc_missingFile", LOC_MISSING_LOG_PATH, CVAR_ARCHIVE);
	Cvar_SetDescription(cl_loc_missingFile, "Path to append missing localization keys");

	cl_loc_trackMissing = Cvar_Get("cl_loc_trackMissing", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_loc_trackMissing, "Track and log missing localization keys");

	Cmd_AddCommand("reloadLanguage", CL_Localize_Reload_f);
	Cmd_AddCommand("loc_test", CL_Localize_Test_f);
	Cmd_AddCommand("loc_test_replace", CL_Localize_TestReplace_f);

	loc_initialized = qtrue;

	Loc_LoadLanguageTables(cl_language->string);
}

void CL_Localize_Shutdown(void)
{
	if (!loc_initialized) {
		return;
	}

	Cmd_RemoveCommand("reloadLanguage");
	Cmd_RemoveCommand("loc_test");
	Cmd_RemoveCommand("loc_test_replace");

	Loc_ClearTable(&loc_current);
	Loc_ClearTable(&loc_defaultLang);
	Loc_ClearTable(&loc_missingKeys);

	loc_initialized = qfalse;
}

void CL_Localize_Frame(void)
{
	qboolean modified = qfalse;

	if (!loc_initialized) {
		return;
	}

	if (cl_language && cl_language->modified) {
		cl_language->modified = qfalse;
		modified = qtrue;
	}

	if (com_language && com_language->modified) {
		com_language->modified = qfalse;
		if (cl_language && com_language->string[0]) {
			Cvar_Set("cl_language", com_language->string);
		}
		modified = qtrue;
	}

	if (modified) {
		Loc_LoadLanguageTables(cl_language ? cl_language->string : loc_defaultCode);
	}
}

qboolean CL_LoadLanguage(const char *languageCode)
{
	if (cl_language && languageCode && languageCode[0]) {
		Cvar_Set("cl_language", languageCode);
	}

	return Loc_LoadLanguageTables(languageCode);
}

const char *CL_Localize(const char *id)
{
	locEntry_t *entry;
	const char *text = id;

	if (!id || !*id) {
		return "";
	}

	entry = Loc_Find(&loc_current, id);
	if (entry) {
		text = entry->value;
	} else {
		// try fallback language
		if (Q_stricmp(loc_current.code, loc_defaultCode) != 0) {
			entry = Loc_Find(&loc_defaultLang, id);
			if (entry) {
				text = entry->value;
			}
		}

		if (!entry) {
			Loc_LogMissing(id);
		}
	}

	return Loc_DebugWrap(id, text);
}

const char *CL_LocalizeFmt(const char *id, ...)
{
	static char buffer[MAX_TRANSLATION_TEXT];
	va_list argptr;
	const char *translated;

	if (!id || !*id) {
		return "";
	}

	translated = CL_Localize(id);

	va_start(argptr, id);
	Q_vsnprintf(buffer, sizeof(buffer), translated, argptr);
	va_end(argptr);

	return buffer;
}

const char *CL_LocalizeReplace(const char *id, const locVar_t *vars, int varCount)
{
	static char buffer[MAX_TRANSLATION_TEXT * 2];
	const char *src;
	char *dst = buffer;
	size_t remaining = sizeof(buffer);

	if (!id || !*id) {
		return "";
	}

	src = CL_Localize(id);

	while (*src && remaining > 1) {
		if (*src == '{') {
			const char *end = strchr(src, '}');
			if (end && end > src + 1) {
				char key[64];
				size_t keyLen = (size_t)(end - src - 1);
				int i;
				const char *value = NULL;

				if (keyLen >= sizeof(key)) {
					keyLen = sizeof(key) - 1;
				}

				Q_strncpyz(key, src + 1, keyLen + 1);

				for (i = 0; vars && i < varCount; i++) {
					if (vars[i].key && Q_stricmp(vars[i].key, key) == 0) {
						value = vars[i].value ? vars[i].value : "";
						break;
					}
				}

				if (value) {
					size_t vlen = strlen(value);
					if (vlen >= remaining) {
						vlen = remaining - 1;
					}
					memcpy(dst, value, vlen);
					dst += vlen;
					remaining -= vlen;
				}

				src = end + 1;
				continue;
			}
		}

		*dst++ = *src++;
		remaining--;
	}

	*dst = '\0';
	return buffer;
}

// ---------------------------------------------------------------------------
// Legacy wrappers (keep old API stable)
// ---------------------------------------------------------------------------

void I18n_Init(void)
{
	CL_Localize_Init();
}

void I18n_Shutdown(void)
{
	CL_Localize_Shutdown();
}

qboolean I18n_LoadLanguage(const char *languageCode)
{
	return CL_LoadLanguage(languageCode);
}

qboolean I18n_LoadLanguageFile(const char *filename)
{
	// Merge an additional file into the active language (useful for mods or hot reload)
	qboolean loaded = qfalse;

	if (!loc_initialized) {
		return qfalse;
	}

	loaded = Loc_LoadJsonFile(filename, &loc_current);
	if (!loaded) {
		loaded = Loc_ParseLegacyIni(filename, &loc_current);
	}

	return loaded;
}

const char *I18n_Translate(const char *key)
{
	return CL_Localize(key);
}

const char *I18n_TranslateFormat(const char *key, ...)
{
	static char buffer[MAX_TRANSLATION_TEXT];
	va_list argptr;
	const char *translated;

	if (!key || !*key) {
		return "";
	}

	translated = CL_Localize(key);

	va_start(argptr, key);
	Q_vsnprintf(buffer, sizeof(buffer), translated, argptr);
	va_end(argptr);

	return buffer;
}

void I18n_SetLanguage(const char *languageCode)
{
	CL_LoadLanguage(languageCode);
}

const char *I18n_GetCurrentLanguage(void)
{
	if (loc_activeCode[0]) {
		return loc_activeCode;
	}
	return loc_defaultCode;
}

qboolean I18n_LanguageExists(const char *languageCode)
{
	fileHandle_t f;
	char filename[MAX_QPATH];
	int len;
	char normalized[MAX_LANGUAGE_CODE];

	Loc_NormalizeLanguageCode(languageCode, normalized, sizeof(normalized));

	Com_sprintf(filename, sizeof(filename), "lang/lang_%s.json", normalized);
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len > 0 && f != FS_INVALID_HANDLE) {
		FS_FCloseFile(f);
		return qtrue;
	}

	// Legacy INI checks
	Com_sprintf(filename, sizeof(filename), "translations/%s.ini", normalized);
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len > 0 && f != FS_INVALID_HANDLE) {
		FS_FCloseFile(f);
		return qtrue;
	}

	Com_sprintf(filename, sizeof(filename), "lang/lang_%s.ini", normalized);
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len > 0 && f != FS_INVALID_HANDLE) {
		FS_FCloseFile(f);
		return qtrue;
	}

	return qfalse;
}

int I18n_GetLanguageCount(void)
{
	int countJson = 0;
	int countIni = 0;
	char **files;

	files = FS_ListFiles("lang", ".json", &countJson);
	if (files) {
		FS_FreeFileList(files);
	}

	files = FS_ListFiles("lang", ".ini", &countIni);
	if (files) {
		FS_FreeFileList(files);
	}

	// Use max to avoid double-counting language codes that have both formats
	return countJson > countIni ? countJson : countIni;
}

void I18n_ListLanguages(void)
{
	int count;
	int i;
	char **files;

	Com_Printf("Available languages (lang/lang_<code>.json):\n");
	files = FS_ListFiles("lang", ".json", &count);
	if (files && count > 0) {
		for (i = 0; i < count; i++) {
			char code[MAX_LANGUAGE_CODE];
			char *dot;

			if (!files[i]) {
				continue;
			}

			Q_strncpyz(code, files[i], sizeof(code));
			dot = strchr(code, '.');
			if (dot) {
				*dot = '\0';
			}

			// Trim "lang_" prefix if present
			if (!Q_strncmp(code, "lang_", 5)) {
				memmove(code, code + 5, strlen(code + 5) + 1);
			}

			Com_Printf("  %s\n", code);
		}
		FS_FreeFileList(files);
	} else {
		Com_Printf("  (none found)\n");
	}
}

