#ifndef __I18N_H__
#define __I18N_H__

#include "q_shared.h"

#define MAX_LANGUAGE_NAME		32
#define MAX_LANGUAGE_CODE		8
#define MAX_TRANSLATION_KEY		256
#define MAX_TRANSLATION_TEXT	1024

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

// i18n functions
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
#define I18n(key) I18n_Translate(key)

#endif // __I18N_H__

