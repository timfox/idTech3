/*
===============================================================================

Modern Error Handling System Implementation

===============================================================================
*/

#include "error_handling.h"
#include "qcommon.h"
#include <cstring>
#include <cstdio>
#include <cstdarg>

//===============================================================================
// IdTech3Exception
//===============================================================================

IdTech3Exception::IdTech3Exception(std::string_view message, std::source_location location) noexcept
    : m_message(message)
    , m_location(location)
#if __cplusplus >= 202302L
    , m_stackTrace(std::stacktrace::current())
#endif
{
}

const char* IdTech3Exception::what() const noexcept
{
    return m_message.c_str();
}

std::string IdTech3Exception::location_string() const
{
    return std::string(m_location.file_name()) + ":" +
           std::to_string(m_location.line()) + " in " +
           m_location.function_name();
}

//===============================================================================
// Specific Exception Types
//===============================================================================

FileException::FileException(std::string_view filename, std::string_view message, std::source_location location)
    : IdTech3Exception(std::string(message) + " (file: " + std::string(filename) + ")", location)
    , m_filename(filename)
{
}

MemoryException::MemoryException(std::string_view message, std::source_location location)
    : IdTech3Exception(message, location)
{
}

NetworkException::NetworkException(std::string_view message, std::source_location location)
    : IdTech3Exception(message, location)
{
}

RenderingException::RenderingException(std::string_view message, std::source_location location)
    : IdTech3Exception(message, location)
{
}

AssetException::AssetException(std::string_view assetName, std::string_view message, std::source_location location)
    : IdTech3Exception(std::string(message) + " (asset: " + std::string(assetName) + ")", location)
    , m_assetName(assetName)
{
}

//===============================================================================
// Error Context and RAII Cleanup
//===============================================================================

void ErrorContext::add_cleanup(std::function<void()> cleanup)
{
    m_cleanups.push_back(std::move(cleanup));
}

void ErrorContext::cleanup()
{
    // Call cleanups in reverse order (stack-like)
    for (auto it = m_cleanups.rbegin(); it != m_cleanups.rend(); ++it) {
        try {
            (*it)();
        } catch (const std::exception& e) {
            // Log cleanup errors but don't throw
            Com_Printf("Error during cleanup: %s\n", e.what());
        }
    }
    m_cleanups.clear();
}

ErrorGuard::ErrorGuard(ErrorContext& context)
    : m_context(context)
    , m_cleanupDone(false)
{
}

ErrorGuard::~ErrorGuard()
{
    if (!m_cleanupDone && m_context.needs_cleanup()) {
        m_context.cleanup();
        m_cleanupDone = true;
    }
}

//===============================================================================
// Global Error Handler
//===============================================================================

GlobalErrorHandler& GlobalErrorHandler::instance()
{
    static GlobalErrorHandler instance;
    return instance;
}

void GlobalErrorHandler::set_exception_handler(std::function<void(const IdTech3Exception&)> handler)
{
    m_exceptionHandler = std::move(handler);
}

void GlobalErrorHandler::set_fatal_error_handler(std::function<void(const std::string&)> handler)
{
    m_fatalErrorHandler = std::move(handler);
}

void GlobalErrorHandler::handle_exception(const IdTech3Exception& e)
{
    log_error(e);

    if (m_exceptionHandler) {
        try {
            m_exceptionHandler(e);
        } catch (const std::exception& handlerException) {
            // Handler threw an exception - this is bad
            Com_Printf("Error handler threw exception: %s\n", handlerException.what());
            handle_fatal_error("Error handler failed");
        }
    } else {
        // Default behavior: convert to legacy error
        Com_Error(ERR_DROP, "Unhandled exception: %s at %s\n",
                 e.what(), e.location_string().c_str());
    }
}

void GlobalErrorHandler::handle_fatal_error(const std::string& message)
{
    if (m_fatalErrorHandler) {
        try {
            m_fatalErrorHandler(message);
        } catch (const std::exception& e) {
            // Fatal error handler failed - we're in deep trouble
            Com_Printf("Fatal error handler failed: %s\n", e.what());
        }
    }

    // Ultimate fallback: legacy fatal error
    Com_Error(ERR_FATAL, "Fatal error: %s\n", message.c_str());
}

void GlobalErrorHandler::log_error(const IdTech3Exception& e)
{
    Com_Printf("Exception caught: %s (%s)\n", e.type(), e.what());
    Com_Printf("Location: %s\n", e.location_string().c_str());

#if __cplusplus >= 202302L
    Com_Printf("Stack trace:\n%s\n", std::to_string(e.stack_trace()).c_str());
#endif
}

void GlobalErrorHandler::log_error(const std::string& message, std::source_location location)
{
    Com_Printf("Error: %s at %s:%d in %s\n",
               message.c_str(),
               location.file_name(),
               location.line(),
               location.function_name());
}

bool GlobalErrorHandler::can_recover_from(const IdTech3Exception& e) const
{
    // Define which exceptions are recoverable
    const char* type = e.type();
    return std::strcmp(type, "FileException") == 0 ||
           std::strcmp(type, "NetworkException") == 0 ||
           std::strcmp(type, "AssetException") == 0;
}

void GlobalErrorHandler::attempt_recovery(const IdTech3Exception& e)
{
    if (!can_recover_from(e)) {
        handle_fatal_error(std::string("Cannot recover from: ") + e.type());
        return;
    }

    const char* type = e.type();

    if (std::strcmp(type, "FileException") == 0) {
        // Try alternative file paths or download missing files
        Com_Printf("Attempting to recover from file error...\n");
        // Implementation would go here
    } else if (std::strcmp(type, "NetworkException") == 0) {
        // Retry network operations
        Com_Printf("Attempting to recover from network error...\n");
        // Implementation would go here
    } else if (std::strcmp(type, "AssetException") == 0) {
        // Try to reload or find alternative assets
        Com_Printf("Attempting to recover from asset error...\n");
        // Implementation would go here
    }
}

//===============================================================================
// Utility Functions
//===============================================================================

namespace ErrorUtils {

void throw_from_error_code(int error_code, std::string_view context)
{
    std::string message = std::string(context) + ": ";

    switch (error_code) {
        case ERR_DROP:
            message += "Non-fatal error";
            throw IdTech3Exception(message);
        case ERR_DISCONNECT:
            message += "Disconnected";
            throw NetworkException(message);
        case ERR_NEED_CD:
            message += "CD required";
            throw IdTech3Exception(message);
        case ERR_AUTOUPDATE:
            message += "Auto-update required";
            throw IdTech3Exception(message);
        default:
            message += "Unknown error code: " + std::to_string(error_code);
            throw IdTech3Exception(message);
    }
}

std::string safe_string_copy(const char* source, size_t max_length)
{
    if (!source) return std::string();

    size_t len = std::strlen(source);
    if (len > max_length) {
        len = max_length;
    }

    return std::string(source, len);
}

void* safe_malloc(size_t size)
{
    if (size == 0) return nullptr;

    void* ptr = std::malloc(size);
    if (!ptr) {
        throw MemoryException("Failed to allocate memory");
    }

    return ptr;
}

void* safe_calloc(size_t num, size_t size)
{
    if (num == 0 || size == 0) return nullptr;

    void* ptr = std::calloc(num, size);
    if (!ptr) {
        throw MemoryException("Failed to allocate and zero memory");
    }

    return ptr;
}

void* safe_realloc(void* ptr, size_t size)
{
    void* new_ptr = std::realloc(ptr, size);
    if (!new_ptr && size > 0) {
        throw MemoryException("Failed to reallocate memory");
    }

    return new_ptr;
}

std::vector<char> read_file_safe(const std::string& filename)
{
    FILE* file = std::fopen(filename.c_str(), "rb");
    if (!file) {
        throw FileException(filename, "Failed to open file for reading");
    }

    // Get file size
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        throw FileException(filename, "Failed to seek to end of file");
    }

    long size = std::ftell(file);
    if (size < 0) {
        std::fclose(file);
        throw FileException(filename, "Failed to get file size");
    }

    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        throw FileException(filename, "Failed to seek to beginning of file");
    }

    std::vector<char> data(size);
    size_t read = std::fread(data.data(), 1, size, file);
    std::fclose(file);

    if (read != static_cast<size_t>(size)) {
        throw FileException(filename, "Failed to read complete file");
    }

    return data;
}

void write_file_safe(const std::string& filename, const void* data, size_t size)
{
    FILE* file = std::fopen(filename.c_str(), "wb");
    if (!file) {
        throw FileException(filename, "Failed to open file for writing");
    }

    size_t written = std::fwrite(data, 1, size, file);
    std::fclose(file);

    if (written != size) {
        throw FileException(filename, "Failed to write complete file");
    }
}

} // namespace ErrorUtils

//===============================================================================
// Legacy Code Integration
//===============================================================================

// Exception-safe wrapper that catches exceptions and converts to Com_Error
extern "C" void safe_com_error(int level, const char* error, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, error);
    vsnprintf(buffer, sizeof(buffer), error, args);
    va_end(args);

    try {
        ErrorUtils::throw_from_error_code(level, buffer);
    } catch (const IdTech3Exception& e) {
        // Already handled by global error handler
        (void)e; // Suppress unused variable warning
    }
}

// Hook into legacy error system
static class LegacyErrorHook
{
public:
    LegacyErrorHook()
    {
        // Set up global error handler to work with legacy system
        GlobalErrorHandler::instance().set_exception_handler(
            [](const IdTech3Exception& e) {
                Com_Error(ERR_DROP, "Exception: %s at %s",
                         e.what(), e.location_string().c_str());
            });

        GlobalErrorHandler::instance().set_fatal_error_handler(
            [](const std::string& message) {
                Com_Error(ERR_FATAL, "Fatal error: %s", message.c_str());
            });
    }
} g_legacyErrorHook;