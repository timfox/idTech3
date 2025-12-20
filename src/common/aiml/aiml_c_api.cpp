/*
=============================================================================
 C bridge for the AIML interpreter
=============================================================================
*/

#ifdef USE_AIML

#include "aiml_c_api.h"

#include <memory>
#include <string>

#include "../qcommon.h"
#include "aiml_interpreter.h"

namespace
{
	std::unique_ptr<aiml::AimlInterpreter> g_aiml;

	aiml::AimlInterpreter* EnsureInterpreter()
	{
		if (!g_aiml) {
			g_aiml = std::make_unique<aiml::AimlInterpreter>();
		}
		return g_aiml.get();
	}
}

extern "C" {

void AIML_Init(void)
{
	EnsureInterpreter();
}

void AIML_Shutdown(void)
{
	g_aiml.reset();
}

qboolean AIML_LoadBuffer(const char* name, const char* buffer)
{
	if (!name || !buffer) {
		return qfalse;
	}

	auto* engine = EnsureInterpreter();
	if (!engine->LoadFromString(name, buffer)) {
		Com_Printf("AIML: failed to parse '%s'\n", name);
		return qfalse;
	}

	Com_Printf("AIML: loaded '%s'\n", name);
	return qtrue;
}

qboolean AIML_LoadFile(const char* path)
{
	if (!path || !*path) {
		return qfalse;
	}

	fileHandle_t f;
	int len = FS_FOpenFileRead(path, &f, qfalse);
	if (len <= 0 || f == FS_INVALID_HANDLE) {
		Com_Printf("AIML: could not open %s\n", path);
		return qfalse;
	}

	std::string contents;
	contents.resize(static_cast<size_t>(len));
	FS_Read(contents.data(), len, f);
	FS_FCloseFile(f);

	return AIML_LoadBuffer(path, contents.c_str());
}

qboolean AIML_Respond(const char* userId, const char* input, char* outBuffer, size_t outSize)
{
	if (!userId || !input || !outBuffer || outSize == 0) {
		return qfalse;
	}

	auto* engine = EnsureInterpreter();
	std::string reply = engine->Reply(userId, input);
	Q_strncpyz(outBuffer, reply.c_str(), outSize);
	return reply.empty() ? qfalse : qtrue;
}

void AIML_SetBotPredicate(const char* key, const char* value)
{
	if (!key || !value) {
		return;
	}
	EnsureInterpreter()->SetBotPredicate(key, value);
}

int AIML_GetBotPredicate(const char* key, char* outBuffer, size_t outSize)
{
	if (!key || !outBuffer || outSize == 0) {
		return 0;
	}
	auto* engine = EnsureInterpreter();
	std::string value = engine->GetBotPredicate(key);
	Q_strncpyz(outBuffer, value.c_str(), outSize);
	return static_cast<int>(value.size());
}

void AIML_ResetSession(const char* userId)
{
	if (!userId) {
		return;
	}
	EnsureInterpreter()->ResetSession(userId);
}

void AIML_ResetAllSessions(void)
{
	EnsureInterpreter()->ResetAllSessions();
}

} // extern "C"

#endif // USE_AIML

