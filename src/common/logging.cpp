/*
===============================================================================

Modern Logging and Diagnostics System Implementation

===============================================================================
*/

#include "logging.h"
#include "qcommon.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>

//===============================================================================
// LogEntry Implementation
//===============================================================================

void LogEntry::formatMessage()
{
    // Format: [YYYY-MM-DD HH:MM:SS] [LEVEL] [CATEGORY] [THREAD] message (file:line)
    std::ostringstream oss;

    auto time = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm = *std::localtime(&time);

    oss << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ");

    switch (level) {
        case LogLevel::Trace: oss << "[TRACE] "; break;
        case LogLevel::Debug: oss << "[DEBUG] "; break;
        case LogLevel::Info: oss << "[INFO] "; break;
        case LogLevel::Warning: oss << "[WARN] "; break;
        case LogLevel::Error: oss << "[ERROR] "; break;
        case LogLevel::Fatal: oss << "[FATAL] "; break;
        default: oss << "[UNKNOWN] "; break;
    }

    switch (category) {
        case LogCategory::Engine: oss << "[ENGINE] "; break;
        case LogCategory::Renderer: oss << "[RENDER] "; break;
        case LogCategory::Audio: oss << "[AUDIO] "; break;
        case LogCategory::Network: oss << "[NET] "; break;
        case LogCategory::FileSystem: oss << "[FS] "; break;
        case LogCategory::UI: oss << "[UI] "; break;
        case LogCategory::Game: oss << "[GAME] "; break;
        case LogCategory::Performance: oss << "[PERF] "; break;
        case LogCategory::Memory: oss << "[MEM] "; break;
        case LogCategory::Threading: oss << "[THREAD] "; break;
        default: oss << "[GENERAL] "; break;
    }

    oss << "[" << threadId << "] " << message;

    if (location.line() != 0) {
        oss << " (" << location.file_name() << ":" << location.line() << ")";
    }

    formattedMessage = oss.str();
}

//===============================================================================
// ConsoleSink Implementation
//===============================================================================

void ConsoleSink::log(const LogEntry& entry)
{
    // Use appropriate console output based on level
    switch (entry.level) {
        case LogLevel::Error:
        case LogLevel::Fatal:
            std::cerr << entry.formattedMessage << std::endl;
            break;
        default:
            std::cout << entry.formattedMessage << std::endl;
            break;
    }
}

//===============================================================================
// FileSink Implementation
//===============================================================================

FileSink::FileSink(const std::string& filename, size_t maxSize, size_t maxFiles)
    : m_filename(filename)
    , m_maxSize(maxSize)
    , m_maxFiles(maxFiles)
    , m_file(nullptr, &std::fclose)
{
    openFile();
}

FileSink::~FileSink()
{
    flush();
}

void FileSink::log(const LogEntry& entry)
{
    std::lock_guard<std::mutex> lock(m_fileMutex);

    if (!m_file) {
        openFile();
        if (!m_file) return;
    }

    // Check if we need to rotate
    std::fseek(m_file.get(), 0, SEEK_END);
    if (static_cast<size_t>(std::ftell(m_file.get())) >= m_maxSize) {
        rotateFiles();
        openFile();
        if (!m_file) return;
    }

    std::fputs(entry.formattedMessage.c_str(), m_file.get());
    std::fputc('\n', m_file.get());
}

void FileSink::flush()
{
    std::lock_guard<std::mutex> lock(m_fileMutex);
    if (m_file) {
        std::fflush(m_file.get());
    }
}

void FileSink::rotateFiles()
{
    // Close current file
    m_file.reset();

    // Rotate existing files
    for (size_t i = m_maxFiles; i > 0; --i) {
        std::string oldName = m_filename + (i == 1 ? "" : "." + std::to_string(i - 1));
        std::string newName = m_filename + "." + std::to_string(i);

        if (std::filesystem::exists(oldName)) {
            std::filesystem::rename(oldName, newName);
        }
    }
}

void FileSink::openFile()
{
    m_file.reset(std::fopen(m_filename.c_str(), "a"));
    if (!m_file) {
        // Fallback to stderr for file errors
        std::cerr << "Failed to open log file: " << m_filename << std::endl;
    }
}

//===============================================================================
// NetworkSink Implementation
//===============================================================================

NetworkSink::NetworkSink(const std::string& endpoint)
    : m_endpoint(endpoint)
{
    // Network initialization would go here
}

NetworkSink::~NetworkSink()
{
    // Network cleanup would go here
}

void NetworkSink::log(const LogEntry& entry)
{
    // Network logging implementation would go here
    // For now, just output to console as placeholder
    std::cout << "[NETWORK] " << entry.formattedMessage << std::endl;
}

//===============================================================================
// Logger Implementation
//===============================================================================

Logger::Logger(std::string_view name, LogCategory category)
    : m_name(name)
    , m_category(category)
    , m_level(LogLevel::Info)
{
}

void Logger::trace(std::string_view message, std::source_location location)
{
    log(LogLevel::Trace, message, location);
}

void Logger::debug(std::string_view message, std::source_location location)
{
    log(LogLevel::Debug, message, location);
}

void Logger::info(std::string_view message, std::source_location location)
{
    log(LogLevel::Info, message, location);
}

void Logger::warning(std::string_view message, std::source_location location)
{
    log(LogLevel::Warning, message, location);
}

void Logger::error(std::string_view message, std::source_location location)
{
    log(LogLevel::Error, message, location);
}

void Logger::fatal(std::string_view message, std::source_location location)
{
    log(LogLevel::Fatal, message, location);
}

void Logger::log(LogLevel level, std::string_view message, std::source_location location)
{
    if (level < m_level) return;

    LogEntry entry(level, m_category, message, m_name, location);
    LoggingSystem::instance().log(entry);
}

//===============================================================================
// LoggingSystem Implementation
//===============================================================================

LoggingSystem& LoggingSystem::instance()
{
    static LoggingSystem instance;
    return instance;
}

LoggingSystem::LoggingSystem()
    : m_running(true)
    , m_queueCapacity(1000)
{
    // Start async logging thread
    m_asyncThread = std::thread([this]() { asyncLoggingThread(); });
}

LoggingSystem::~LoggingSystem()
{
    shutdown();
}

void LoggingSystem::configure(const LogConfig& config)
{
    m_config = config;
    m_queueCapacity = config.queueSize;

    // Reconfigure sinks based on new config
    clearSinks();

    if (config.enableConsole) {
        addSink(std::make_unique<ConsoleSink>());
    }

    if (config.enableFile) {
        addSink(std::make_unique<FileSink>(config.logFile, config.maxFileSize, config.maxFiles));
    }

    if (config.enableNetwork && !config.networkEndpoint.empty()) {
        addSink(std::make_unique<NetworkSink>(config.networkEndpoint));
    }
}

void LoggingSystem::addSink(std::unique_ptr<LogSink> sink)
{
    m_sinks.push_back(std::move(sink));
}

void LoggingSystem::removeSink(const std::string& name)
{
    m_sinks.erase(
        std::remove_if(m_sinks.begin(), m_sinks.end(),
                      [&](const std::unique_ptr<LogSink>& sink) {
                          return sink->name() == name;
                      }),
        m_sinks.end());
}

void LoggingSystem::clearSinks()
{
    m_sinks.clear();
}

Logger& LoggingSystem::getLogger(std::string_view name)
{
    auto it = m_loggers.find(std::string(name));
    if (it == m_loggers.end()) {
        auto logger = std::make_unique<Logger>(name);
        it = m_loggers.emplace(std::string(name), std::move(logger)).first;
    }
    return *it->second;
}

void LoggingSystem::setDefaultLevel(LogLevel level)
{
    m_config.level = level;
    for (auto& loggerPair : m_loggers) {
        loggerPair.second->setLevel(level);
    }
}

void LoggingSystem::log(const LogEntry& entry)
{
    if (entry.level < m_config.level) return;

    if (m_config.asyncLogging) {
        // Async logging
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondition.wait(lock, [this]() {
            return m_logQueue.size() < m_queueCapacity || !m_running;
        });

        if (!m_running) return;

        m_logQueue.push_back(entry);
        lock.unlock();
        m_queueCondition.notify_one();
    } else {
        // Synchronous logging
        processLogEntry(entry);
    }

    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalEntries++;
    }
}

void LoggingSystem::flush()
{
    for (auto& sink : m_sinks) {
        sink->flush();
    }
}

LoggingSystem::LogStats LoggingSystem::stats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void LoggingSystem::shutdown()
{
    m_running = false;
    m_queueCondition.notify_all();

    if (m_asyncThread.joinable()) {
        m_asyncThread.join();
    }

    flush();
}

void LoggingSystem::processLogEntry(const LogEntry& entry)
{
    for (auto& sink : m_sinks) {
        try {
            sink->log(entry);
        } catch (const std::exception& e) {
            // Don't let sink failures crash the logging system
            std::cerr << "Log sink error: " << e.what() << std::endl;
        }
    }
}

void LoggingSystem::asyncLoggingThread()
{
    while (m_running) {
        std::vector<LogEntry> entries;

        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_queueCondition.wait(lock, [this]() {
                return !m_logQueue.empty() || !m_running;
            });

            if (!m_running && m_logQueue.empty()) break;

            // Move entries to local vector for processing
            entries.swap(m_logQueue);
        }

        // Process entries
        for (const auto& entry : entries) {
            processLogEntry(entry);
        }

        // Update queue size stat
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.queueSize = m_logQueue.size();
        }
    }
}

//===============================================================================
// PerformanceProfiler Implementation
//===============================================================================

PerformanceProfiler& PerformanceProfiler::instance()
{
    static PerformanceProfiler instance;
    return instance;
}

void PerformanceProfiler::beginSession(std::string_view name)
{
    // Implementation for profiling session management
    LOG_INFO_FMT("Started profiling session: {}", name);
}

void PerformanceProfiler::endSession()
{
    LOG_INFO("Ended profiling session");
    exportStats("profile_results.json");
}

PerformanceProfiler::ScopedProfile::ScopedProfile(std::string_view name, std::source_location location)
    : m_name(name)
    , m_location(location)
{
    PerformanceProfiler::instance().beginProfile(name);
}

PerformanceProfiler::ScopedProfile::~ScopedProfile()
{
    PerformanceProfiler::instance().endProfile(m_name);
}

void PerformanceProfiler::beginProfile(std::string_view name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string nameStr(name);
    auto& data = m_activeProfiles[nameStr];
    data.name = nameStr;
    data.start = std::chrono::high_resolution_clock::now();
}

void PerformanceProfiler::endProfile(std::string_view name)
{
    auto end = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);

    std::string nameStr(name);
    auto it = m_activeProfiles.find(nameStr);
    if (it != m_activeProfiles.end()) {
        auto& data = it->second;
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - data.start);

        data.callCount++;
        data.totalTime += duration;
        data.minTime = std::min(data.minTime, duration);
        data.maxTime = std::max(data.maxTime, duration);
    }
}

std::vector<PerformanceProfiler::ProfileStats> PerformanceProfiler::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ProfileStats> stats;
    stats.reserve(m_activeProfiles.size());

    for (const auto& [name, data] : m_activeProfiles) {
        ProfileStats stat;
        stat.name = name;
        stat.callCount = data.callCount;
        stat.totalTime = data.totalTime;
        stat.minTime = data.minTime;
        stat.maxTime = data.maxTime;
        if (data.callCount > 0) {
            stat.avgTime = data.totalTime / data.callCount;
        }
        stats.push_back(stat);
    }

    return stats;
}

void PerformanceProfiler::resetStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeProfiles.clear();
}

void PerformanceProfiler::exportStats(const std::string& filename) const
{
    auto stats = getStats();

    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR_FMT("Failed to open profile export file: {}", filename);
        return;
    }

    file << "{\n";
    file << "  \"profiling_results\": [\n";

    for (size_t i = 0; i < stats.size(); ++i) {
        const auto& stat = stats[i];
        file << "    {\n";
        file << "      \"name\": \"" << stat.name << "\",\n";
        file << "      \"call_count\": " << stat.callCount << ",\n";
        file << "      \"total_time_us\": " << stat.totalTime.count() << ",\n";
        file << "      \"min_time_us\": " << stat.minTime.count() << ",\n";
        file << "      \"max_time_us\": " << stat.maxTime.count() << ",\n";
        file << "      \"avg_time_us\": " << stat.avgTime.count() << "\n";
        file << "    }" << (i < stats.size() - 1 ? "," : "") << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    LOG_INFO_FMT("Exported profiling stats to: {}", filename);
}

//===============================================================================
// Legacy Compatibility Implementation
//===============================================================================

void Com_Printf_Compat(const char* msg, ...)
{
    va_list args;
    va_start(args, msg);

    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), msg, args);

    va_end(args);

    // Forward to modern logging system
    LOG_INFO(buffer);
}

void Com_DPrintf_Compat(const char* msg, ...)
{
    va_list args;
    va_start(args, msg);

    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), msg, args);

    va_end(args);

    // Forward to modern logging system
    LOG_DEBUG(buffer);
}

void Com_Error_Compat(int level, const char* error, ...)
{
    va_list args;
    va_start(args, error);

    char buffer[4096];
    vsnprintf(buffer, sizeof(buffer), error, args);

    va_end(args);

    // Convert to modern logging
    switch (level) {
        case ERR_DROP:
            LOG_ERROR(buffer);
            break;
        case ERR_DISCONNECT:
            LOG_WARNING(buffer);
            break;
        default:
            LOG_FATAL(buffer);
            break;
    }
}

//===============================================================================
// Initialization
//===============================================================================

static class LoggingSystemInitializer
{
public:
    LoggingSystemInitializer()
    {
        // Initialize default logging configuration
        LogConfig config;
        config.enableConsole = true;
        config.enableFile = true;
        config.logFile = "idtech3.log";

        LoggingSystem::instance().configure(config);

        LOG_INFO("Modern logging system initialized");
    }
} g_loggingInitializer;