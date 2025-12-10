#ifndef __I18N_H__
#define __I18N_H__

#include "q_shared.h"

#define MAX_LANGUAGE_NAME		32
#define MAX_LANGUAGE_CODE		8
#define MAX_TRANSLATION_KEY		256
#define MAX_TRANSLATION_TEXT	1024
#define DEFAULT_LANGUAGE_CODE	LANGUAGE_ENGLISH

typedef struct locVar_s {
	const char *key;
	const char *value;
} locVar_t;

// Language codes (ISO 639-1)
#define LANGUAGE_ENGLISH		"en"
#define LANGUAGE_FRENCH		"fr"
#define LANGUAGE_GERMAN		"de"
#define LANGUAGE_SPANISH	"es"
#define LANGUAGE_ITALIAN	"it"
#define LANGUAGE_RUSSIAN	"ru"
#define LANGUAGE_POLISH		"pl"
#define LANGUAGE_CZECH		"cs"

typedef struct translationEntry_s {
	char key[MAX_TRANSLATION_KEY];
	char text[MAX_TRANSLATION_TEXT];
	struct translationEntry_s *next;
} translationEntry_t;

typedef struct language_s {
	char code[MAX_LANGUAGE_CODE];
	char name[MAX_LANGUAGE_NAME];
	translationEntry_t *entries;
	int numEntries;
	struct language_s *next;
} language_t;

// Modern localization API (preferred)
void		CL_Localize_Init(void);
void		CL_Localize_Shutdown(void);
void		CL_Localize_Frame(void);
qboolean	CL_LoadLanguage(const char *languageCode); // "en", "fr", "pl", etc.
const char *CL_Localize(const char *id);
const char *CL_LocalizeFmt(const char *id, ...);
const char *CL_LocalizeReplace(const char *id, const locVar_t *vars, int varCount);

// Legacy-compatible i18n entry points (forward to the modern API)
void		I18n_Init(void);
void		I18n_Shutdown(void);
qboolean	I18n_LoadLanguage(const char *languageCode);
qboolean	I18n_LoadLanguageFile(const char *filename);
const char *I18n_Translate(const char *key);
const char *I18n_TranslateFormat(const char *key, ...);
void		I18n_SetLanguage(const char *languageCode);
const char *I18n_GetCurrentLanguage(void);
qboolean	I18n_LanguageExists(const char *languageCode);
int			I18n_GetLanguageCount(void);
void		I18n_ListLanguages(void);

// Helper macro for translation
#define I18n(key) CL_Localize(key)

#endif // __I18N_H__

