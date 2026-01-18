/*
===============================================================================

Modern Error Handling System for id Tech 3

Provides exception-based error handling with proper RAII cleanup.

===============================================================================
*/

#pragma once

#include "q_shared.h"
#include <exception>
#include <string>
#include <string_view>
#include <source_location>
#include <memory>
#include <vector>
#include <functional>

// Stack trace support (C++23)
#if __cplusplus >= 202302L
#include <stacktrace>
#endif

//===============================================================================
// Exception Classes
//===============================================================================

class IdTech3Exception : public std::exception
{
public:
    explicit IdTech3Exception(std::string_view message,
                             std::source_location location = std::source_location::current()) noexcept;

    IdTech3Exception(const IdTech3Exception&) noexcept = default;
    IdTech3Exception& operator=(const IdTech3Exception&) noexcept = default;

    virtual ~IdTech3Exception() override = default;

    // Exception interface
    virtual const char* what() const noexcept override;
    virtual const char* type() const noexcept { return "IdTech3Exception"; }

    // Location information
    const std::source_location& location() const noexcept { return m_location; }
    std::string location_string() const;

    // Stack trace (if available)
#if __cplusplus >= 202302L
    const std::stacktrace& stack_trace() const noexcept { return m_stackTrace; }
#endif

protected:
    std::string m_message;
    std::source_location m_location;
#if __cplusplus >= 202302L
    std::stacktrace m_stackTrace;
#endif
};

// Specific exception types
class FileException : public IdTech3Exception
{
public:
    FileException(std::string_view filename, std::string_view message,
                  std::source_location location = std::source_location::current());

    const char* type() const noexcept override { return "FileException"; }
    const std::string& filename() const noexcept { return m_filename; }

private:
    std::string m_filename;
};

class MemoryException : public IdTech3Exception
{
public:
    explicit MemoryException(std::string_view message,
                            std::source_location location = std::source_location::current());

    const char* type() const noexcept override { return "MemoryException"; }
};

class NetworkException : public IdTech3Exception
{
public:
    explicit NetworkException(std::string_view message,
                             std::source_location location = std::source_location::current());

    const char* type() const noexcept override { return "NetworkException"; }
};

class RenderingException : public IdTech3Exception
{
public:
    explicit RenderingException(std::string_view message,
                               std::source_location location = std::source_location::current());

    const char* type() const noexcept override { return "RenderingException"; }
};

class AssetException : public IdTech3Exception
{
public:
    AssetException(std::string_view assetName, std::string_view message,
                   std::source_location location = std::source_location::current());

    const char* type() const noexcept override { return "AssetException"; }
    const std::string& asset_name() const noexcept { return m_assetName; }

private:
    std::string m_assetName;
};

//===============================================================================
// Error Context and RAII Cleanup
//===============================================================================

class ErrorContext
{
public:
    ErrorContext() = default;
    ~ErrorContext() = default;

    // Add cleanup function to be called on error
    void add_cleanup(std::function<void()> cleanup);

    // Execute all cleanup functions
    void cleanup();

    // Check if cleanup is needed
    bool needs_cleanup() const { return !m_cleanups.empty(); }

private:
    std::vector<std::function<void()>> m_cleanups;
};

// RAII error context guard
class ErrorGuard
{
public:
    explicit ErrorGuard(ErrorContext& context);
    ~ErrorGuard();

    // Prevent copying
    ErrorGuard(const ErrorGuard&) = delete;
    ErrorGuard& operator=(const ErrorGuard&) = delete;

private:
    ErrorContext& m_context;
    bool m_cleanupDone;
};

//===============================================================================
// Error Handling Macros and Functions
//===============================================================================

// Modern TRY-CATCH macros that work with exceptions
#define IDTECH3_TRY try {
#define IDTECH3_CATCH(exception_type) } catch (const exception_type& e) {
#define IDTECH3_CATCH_ALL } catch (const std::exception& e) {
#define IDTECH3_END_TRY }

// Safe resource management with automatic cleanup
#define IDTECH3_SCOPE_GUARD(context) ErrorGuard guard(context)

// Error checking macros
#define IDTECH3_CHECK(condition, exception_type, message) \
    do { \
        if (!(condition)) { \
            throw exception_type(message); \
        } \
    } while (0)

#define IDTECH3_CHECK_NULL(ptr, exception_type, message) \
    IDTECH3_CHECK(ptr != nullptr, exception_type, message)

#define IDTECH3_CHECK_FILE(file, filename) \
    IDTECH3_CHECK(file != nullptr, FileException, std::string("Failed to open file: ") + filename)

// Assertion with exception throwing
#ifdef _DEBUG
#define IDTECH3_ASSERT(condition, exception_type, message) \
    IDTECH3_CHECK(condition, exception_type, message)
#else
#define IDTECH3_ASSERT(condition, exception_type, message) ((void)0)
#endif

//===============================================================================
// Global Error Handler
//===============================================================================

class GlobalErrorHandler
{
public:
    static GlobalErrorHandler& instance();

    // Set custom error handlers
    void set_exception_handler(std::function<void(const IdTech3Exception&)> handler);
    void set_fatal_error_handler(std::function<void(const std::string&)> handler);

    // Handle exceptions
    void handle_exception(const IdTech3Exception& e);
    void handle_fatal_error(const std::string& message);

    // Error logging
    void log_error(const IdTech3Exception& e);
    void log_error(const std::string& message, std::source_location location = std::source_location::current());

    // Error recovery
    bool can_recover_from(const IdTech3Exception& e) const;
    void attempt_recovery(const IdTech3Exception& e);

private:
    GlobalErrorHandler() = default;
    ~GlobalErrorHandler() = default;

    GlobalErrorHandler(const GlobalErrorHandler&) = delete;
    GlobalErrorHandler& operator=(const GlobalErrorHandler&) = delete;

    std::function<void(const IdTech3Exception&)> m_exceptionHandler;
    std::function<void(const std::string&)> m_fatalErrorHandler;
};

//===============================================================================
// Utility Functions
//===============================================================================

namespace ErrorUtils {

// Convert legacy error codes to exceptions
[[noreturn]] void throw_from_error_code(int error_code, std::string_view context = "");

// Safe string operations
std::string safe_string_copy(const char* source, size_t max_length = 4096);

// Safe memory operations
void* safe_malloc(size_t size);
void* safe_calloc(size_t num, size_t size);
void* safe_realloc(void* ptr, size_t size);

// Resource management helpers
template<typename T, typename Deleter = std::default_delete<T>>
using unique_resource = std::unique_ptr<T, Deleter>;

template<typename T>
using resource_ptr = std::shared_ptr<T>;

// File operations with error handling
std::vector<char> read_file_safe(const std::string& filename);
void write_file_safe(const std::string& filename, const void* data, size_t size);

} // namespace ErrorUtils

//===============================================================================
// Integration with Legacy Code
//===============================================================================

// Convert exceptions to legacy error handling
extern "C" {
void Com_Error(int level, const char* error, ...);
void Com_Printf(const char* msg, ...);
}

// Exception-safe wrapper for legacy functions
template<typename Func, typename... Args>
auto safe_call(Func&& func, Args&&... args) {
    try {
        return std::forward<Func>(func)(std::forward<Args>(args)...);
    } catch (const IdTech3Exception& e) {
        GlobalErrorHandler::instance().handle_exception(e);
        // Return default value for function return type
        if constexpr (std::is_same_v<decltype(func(args...)), void>) {
            return;
        } else {
            return decltype(func(args...)){};
        }
    }
}