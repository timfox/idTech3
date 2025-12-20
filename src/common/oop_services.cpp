/*
===========================================================================
OOP Service Locator implementation
===========================================================================
*/

#ifdef __cplusplus

#include "oop_services.h"
#include <string>

namespace Services {

ILogger *ServiceLocator::logger_ = nullptr;
NullLogger ServiceLocator::nullLogger_{};
DefaultEngineLogger ServiceLocator::defaultLogger_{};

void DefaultEngineLogger::log( LogLevel level, std::string_view message ) {
	const char *prefix = "";
	switch ( level ) {
		case LogLevel::Debug: prefix = "[DBG] "; break;
		case LogLevel::Info:  prefix = "[INF] "; break;
		case LogLevel::Warn:  prefix = "[WRN] "; break;
		case LogLevel::Error: prefix = "[ERR] "; break;
	}

	std::string line;
	line.reserve( message.size() + 8 );
	line.append( prefix );
	line.append( message );
	line.push_back( '\n' );

	Com_Printf( "%s", line.c_str() );
}

void ServiceLocator::SetLogger( ILogger *logger ) {
	logger_ = logger ? logger : &defaultLogger_;
}

ILogger &ServiceLocator::Logger() {
	if ( logger_ == nullptr ) {
		logger_ = &defaultLogger_;
	}
	return *logger_;
}

} // namespace Services

#endif // __cplusplus


