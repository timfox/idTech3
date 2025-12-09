/*
===========================================================================
OOP Service Locator and Logger Interfaces

Lightweight C++23-style interfaces to decouple subsystems from global
functions. Uses a default logger that bridges to the existing engine
Com_Printf output.
===========================================================================
*/

#pragma once

#ifdef __cplusplus

#include <string_view>

// Forward declare engine logging hook
extern "C" void Com_Printf( const char *fmt, ... );

namespace Services {

enum class LogLevel {
	Debug,
	Info,
	Warn,
	Error
};

struct ILogger {
	virtual ~ILogger() = default;
	virtual void log( LogLevel level, std::string_view message ) = 0;
};

// Null object to avoid null checks.
class NullLogger final : public ILogger {
public:
	void log( LogLevel, std::string_view ) override {}
};

// Default logger that forwards to the engine print function.
class DefaultEngineLogger final : public ILogger {
public:
	void log( LogLevel level, std::string_view message ) override;
};

// Simple service locator; intentionally minimal to keep usage explicit.
class ServiceLocator {
public:
	static void SetLogger( ILogger *logger );
	static ILogger &Logger();

private:
	static ILogger *logger_;
	static NullLogger nullLogger_;
	static DefaultEngineLogger defaultLogger_;
};

} // namespace Services

#endif // __cplusplus


