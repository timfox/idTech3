/*
===============================================================================

Modern Logging and Diagnostics System for id Tech 3

Provides structured logging, performance diagnostics, and debugging tools.

===============================================================================
*/

#pragma once

#include "q_shared.h"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <chrono>
#include <source_location>
#include <format>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>

//===============================================================================
// Log Levels and Configuration
//===============================================================================

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Fatal = 5,
    Off = 6
};

enum class LogCategory {
    General = 0,
    Engine = 1,
    Renderer = 2,
    Audio = 3,
    Network = 4,
    FileSystem = 5,
    UI = 6,
    Game = 7,
    Performance = 8,
    Memory = 9,
    Threading = 10
};

struct LogConfig {
    LogLevel level = LogLevel::Info;
    bool enableConsole = true;
    bool enableFile = true;
    bool enableNetwork = false;
    std::string logFile = "idtech3.log";
    std::string networkEndpoint;
    size_t maxFileSize = 10 * 1024 * 1024; // 10MB
    size_t maxFiles = 5; // Keep 5 rotated files
    bool asyncLogging = true;
    size_t queueSize = 1000; // Async queue size
};

//===============================================================================
// Log Entry Structure
//===============================================================================

struct LogEntry {
    std::chrono::system_clock::time_point timestamp;
    LogLevel level;
    LogCategory category;
    std::thread::id threadId;
    std::string loggerName;
    std::string message;
    std::source_location location;
    std::string formattedMessage;

    LogEntry(LogLevel lvl, LogCategory cat, std::string_view msg,
             std::string_view logger, std::source_location loc = std::source_location::current())
        : timestamp(std::chrono::system_clock::now())
        , level(lvl)
        , category(cat)
        , threadId(std::this_thread::get_id())
        , loggerName(logger)
        , message(msg)
        , location(loc)
    {
        formatMessage();
    }

private:
    void formatMessage();
};

//===============================================================================
// Log Sink Interface
//===============================================================================

class LogSink {
public:
    virtual ~LogSink() = default;
    virtual void log(const LogEntry& entry) = 0;
    virtual void flush() = 0;
    virtual std::string name() const = 0;
};

// Console sink
class ConsoleSink : public LogSink {
public:
    void log(const LogEntry& entry) override;
    void flush() override {}
    std::string name() const override { return "console"; }
};

// File sink with rotation
class FileSink : public LogSink {
public:
    explicit FileSink(const std::string& filename, size_t maxSize = 10*1024*1024, size_t maxFiles = 5);
    ~FileSink() override;

    void log(const LogEntry& entry) override;
    void flush() override;
    std::string name() const override { return "file"; }

private:
    void rotateFiles();
    void openFile();

    std::string m_filename;
    size_t m_maxSize;
    size_t m_maxFiles;
    std::unique_ptr<std::FILE, decltype(&std::fclose)> m_file;
    std::mutex m_fileMutex;
};

// Network sink (for remote logging)
class NetworkSink : public LogSink {
public:
    explicit NetworkSink(const std::string& endpoint);
    ~NetworkSink() override;

    void log(const LogEntry& entry) override;
    void flush() override;
    std::string name() const override { return "network"; }

private:
    std::string m_endpoint;
    // Network connection would be implemented here
};

//===============================================================================
// Logger Class
//===============================================================================

class Logger {
public:
    explicit Logger(std::string_view name, LogCategory category = LogCategory::General);
    ~Logger() = default;

    // Configuration
    void setLevel(LogLevel level) { m_level = level; }
    LogLevel level() const { return m_level; }

    void setCategory(LogCategory category) { m_category = category; }
    LogCategory category() const { return m_category; }

    const std::string& name() const { return m_name; }

    // Logging methods
    void trace(std::string_view message, std::source_location location = std::source_location::current());
    void debug(std::string_view message, std::source_location location = std::source_location::current());
    void info(std::string_view message, std::source_location location = std::source_location::current());
    void warning(std::string_view message, std::source_location location = std::source_location::current());
    void error(std::string_view message, std::source_location location = std::source_location::current());
    void fatal(std::string_view message, std::source_location location = std::source_location::current());

    // Formatted logging (C++20 format support)
    template<typename... Args>
    void trace(std::format_string<Args...> fmt, Args&&... args,
               std::source_location location = std::source_location::current()) {
        trace(std::format(fmt, std::forward<Args>(args)...), location);
    }

    template<typename... Args>
    void debug(std::format_string<Args...> fmt, Args&&... args,
               std::source_location location = std::source_location::current()) {
        debug(std::format(fmt, std::forward<Args>(args)...), location);
    }

    template<typename... Args>
    void info(std::format_string<Args...> fmt, Args&&... args,
              std::source_location location = std::source_location::current()) {
        info(std::format(fmt, std::forward<Args>(args)...), location);
    }

    template<typename... Args>
    void warning(std::format_string<Args...> fmt, Args&&... args,
                 std::source_location location = std::source_location::current()) {
        warning(std::format(fmt, std::forward<Args>(args)...), location);
    }

    template<typename... Args>
    void error(std::format_string<Args...> fmt, Args&&... args,
               std::source_location location = std::source_location::current()) {
        error(std::format(fmt, std::forward<Args>(args)...), location);
    }

    template<typename... Args>
    void fatal(std::format_string<Args...> fmt, Args&&... args,
               std::source_location location = std::source_location::current()) {
        fatal(std::format(fmt, std::forward<Args>(args)...), location);
    }

private:
    void log(LogLevel level, std::string_view message, std::source_location location);

    std::string m_name;
    LogCategory m_category;
    LogLevel m_level;
};

//===============================================================================
// Logging System Manager
//===============================================================================

class LoggingSystem {
public:
    static LoggingSystem& instance();

    // Configuration
    void configure(const LogConfig& config);
    const LogConfig& config() const { return m_config; }

    // Sink management
    void addSink(std::unique_ptr<LogSink> sink);
    void removeSink(const std::string& name);
    void clearSinks();

    // Logger management
    Logger& getLogger(std::string_view name);
    void setDefaultLevel(LogLevel level);

    // Logging
    void log(const LogEntry& entry);
    void flush();

    // Statistics
    struct LogStats {
        size_t totalEntries = 0;
        size_t droppedEntries = 0;
        size_t queueSize = 0;
        double avgProcessingTime = 0.0; // milliseconds
    };

    LogStats stats() const;

    // Shutdown
    void shutdown();

private:
    LoggingSystem();
    ~LoggingSystem();

    void processLogEntry(const LogEntry& entry);
    void asyncLoggingThread();

    LogConfig m_config;
    std::vector<std::unique_ptr<LogSink>> m_sinks;
    std::unordered_map<std::string, std::unique_ptr<Logger>> m_loggers;

    // Async logging
    std::atomic<bool> m_running;
    std::thread m_asyncThread;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::vector<LogEntry> m_logQueue;
    size_t m_queueCapacity;

    // Statistics
    mutable std::mutex m_statsMutex;
    LogStats m_stats;
};

//===============================================================================
// Performance Diagnostics
//===============================================================================

class PerformanceProfiler {
public:
    static PerformanceProfiler& instance();

    // Profiling session
    void beginSession(std::string_view name);
    void endSession();

    // Scoped profiling
    class ScopedProfile {
    public:
        explicit ScopedProfile(std::string_view name,
                              std::source_location location = std::source_location::current());
        ~ScopedProfile();

    private:
        std::string m_name;
        std::chrono::high_resolution_clock::time_point m_start;
        std::source_location m_location;
    };

    // Manual profiling
    void beginProfile(std::string_view name);
    void endProfile(std::string_view name);

    // Statistics
    struct ProfileStats {
        std::string name;
        size_t callCount = 0;
        std::chrono::microseconds totalTime{0};
        std::chrono::microseconds minTime{std::chrono::microseconds::max()};
        std::chrono::microseconds maxTime{0};
        std::chrono::microseconds avgTime{0};
    };

    std::vector<ProfileStats> getStats() const;
    void resetStats();
    void exportStats(const std::string& filename) const;

private:
    PerformanceProfiler() = default;

    struct ProfileData {
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
        size_t callCount = 0;
        std::chrono::microseconds totalTime{0};
        std::chrono::microseconds minTime{std::chrono::microseconds::max()};
        std::chrono::microseconds maxTime{0};
    };

    std::unordered_map<std::string, ProfileData> m_activeProfiles;
    mutable std::mutex m_mutex;
};

//===============================================================================
// Convenience Macros
//===============================================================================

// Get logger for current module
#define GET_LOGGER() LoggingSystem::instance().getLogger(__func__)

// Logging macros
#define LOG_TRACE(msg) GET_LOGGER().trace(msg)
#define LOG_DEBUG(msg) GET_LOGGER().debug(msg)
#define LOG_INFO(msg) GET_LOGGER().info(msg)
#define LOG_WARNING(msg) GET_LOGGER().warning(msg)
#define LOG_ERROR(msg) GET_LOGGER().error(msg)
#define LOG_FATAL(msg) GET_LOGGER().fatal(msg)

// Formatted logging macros
#define LOG_TRACE_FMT(fmt, ...) GET_LOGGER().trace(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) GET_LOGGER().debug(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_INFO_FMT(fmt, ...) GET_LOGGER().info(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_WARNING_FMT(fmt, ...) GET_LOGGER().warning(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) GET_LOGGER().error(fmt __VA_OPT__(,) __VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...) GET_LOGGER().fatal(fmt __VA_OPT__(,) __VA_ARGS__)

// Performance profiling macros
#define PROFILE_SCOPE(name) PerformanceProfiler::ScopedProfile profile_scope(name)
#define PROFILE_BEGIN(name) PerformanceProfiler::instance().beginProfile(name)
#define PROFILE_END(name) PerformanceProfiler::instance().endProfile(name)

//===============================================================================
// Legacy Compatibility
//===============================================================================

extern "C" {
void Com_Printf(const char* msg, ...);
void Com_DPrintf(const char* msg, ...);
void Com_Error(int level, const char* error, ...);
}

// Legacy compatibility functions
void Com_Printf_Compat(const char* msg, ...);
void Com_DPrintf_Compat(const char* msg, ...);
void Com_Error_Compat(int level, const char* error, ...);

//===============================================================================
// Example Usage
//===============================================================================

/*
Example usage of the modern logging system:

// Basic logging
LOG_INFO("Engine initialized successfully");

// Formatted logging (C++20)
LOG_INFO_FMT("Loaded {} textures in {:.2f} seconds", textureCount, loadTime);

// Performance profiling
void renderScene() {
    PROFILE_SCOPE("renderScene");

    {
        PROFILE_SCOPE("geometry_pass");
        // Render geometry
    }

    {
        PROFILE_SCOPE("lighting_pass");
        // Render lighting
    }
}

// Custom logger
Logger& rendererLogger = LoggingSystem::instance().getLogger("Renderer");
rendererLogger.info("OpenGL version: {}", glGetString(GL_VERSION));

*/