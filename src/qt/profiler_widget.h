/*
===============================================================================

Qt Profiler Widget for id Tech 3

Provides real-time performance monitoring and debugging tools.

===============================================================================
*/

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QTableWidget>
#include <QTreeWidget>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QTimer>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSplitter>
#include <memory>
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>

// Qt Charts includes
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QAreaSeries>

// Performance data structures
struct PerformanceSample {
    std::chrono::steady_clock::time_point timestamp;
    float fps = 0.0f;
    float frameTime = 0.0f; // milliseconds
    uint32_t triangles = 0;
    uint32_t drawCalls = 0;
    uint64_t memoryUsed = 0; // bytes
    float cpuUsage = 0.0f;   // percentage
    float gpuUsage = 0.0f;   // percentage
};

struct ProfilerStats {
    float averageFPS = 0.0f;
    float minFPS = 0.0f;
    float maxFPS = 0.0f;
    float averageFrameTime = 0.0f;
    uint32_t totalSamples = 0;
    uint64_t totalMemoryPeak = 0;
    float cpuUsagePeak = 0.0f;
    float gpuUsagePeak = 0.0f;
};

enum class ProfilerMode {
    RealTime,
    Capture,
    Analysis
};

enum class GraphType {
    FPS,
    FrameTime,
    Memory,
    CPU,
    GPU,
    Triangles,
    DrawCalls
};

// Profiler widget main class
class ProfilerWidget : public QWidget
{
    Q_OBJECT

public:
    ProfilerWidget(QWidget *parent = nullptr);
    ~ProfilerWidget() override;

    // Control
    void startProfiling();
    void stopProfiling();
    void pauseProfiling();
    void resumeProfiling();
    bool isProfiling() const { return m_isProfiling; }

    // Data collection
    void addPerformanceSample(const PerformanceSample& sample);
    void clearData();

    // Settings
    void setSampleRate(int hz);
    int sampleRate() const { return m_sampleRate; }

    void setHistorySize(int seconds);
    int historySize() const { return m_historySizeSeconds; }

    void setProfilerMode(ProfilerMode mode);
    ProfilerMode profilerMode() const { return m_profilerMode; }

    // Export
    void exportData(const QString& filename);
    void exportScreenshot(const QString& filename);

signals:
    void profilingStarted();
    void profilingStopped();
    void dataUpdated();

public slots:
    void updateDisplay();
    void onSampleRateChanged(int rate);
    void onHistorySizeChanged(int seconds);
    void onGraphTypeChanged(int type);
    void onProfilerModeChanged(int mode);

private slots:
    void onStartStopClicked();
    void onPauseResumeClicked();
    void onClearClicked();
    void onExportClicked();
    void onSettingsChanged();

private:
    void setupUI();
    void createToolbar();
    void createStatsPanel();
    void createGraphsPanel();
    void createDetailsPanel();
    void setupConnections();

    // Data management
    void updateStats();
    void pruneOldData();
    PerformanceSample interpolateSample(const PerformanceSample& a, const PerformanceSample& b, float t) const;

    // UI updates
    void updateStatsDisplay();
    void updateGraphs();
    void updateDetailsTable();

    // Utility
    QString formatTime(float milliseconds) const;
    QString formatMemory(uint64_t bytes) const;
    QString formatPercentage(float percentage) const;
    QColor getPerformanceColor(float value, float min, float max) const;

    // UI components
    QVBoxLayout *m_mainLayout = nullptr;
    QSplitter *m_mainSplitter = nullptr;

    // Toolbar
    QHBoxLayout *m_toolbarLayout = nullptr;
    QPushButton *m_startStopButton = nullptr;
    QPushButton *m_pauseResumeButton = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QSpinBox *m_sampleRateSpin = nullptr;
    QSpinBox *m_historySizeSpin = nullptr;

    // Stats panel
    QGroupBox *m_statsGroup = nullptr;
    QGridLayout *m_statsLayout = nullptr;
    QLabel *m_fpsLabel = nullptr;
    QLabel *m_frameTimeLabel = nullptr;
    QLabel *m_memoryLabel = nullptr;
    QLabel *m_cpuLabel = nullptr;
    QLabel *m_gpuLabel = nullptr;
    QLabel *m_trianglesLabel = nullptr;
    QLabel *m_drawCallsLabel = nullptr;

    // Min/Max/Avg displays
    QLabel *m_fpsMinMaxLabel = nullptr;
    QLabel *m_memoryPeakLabel = nullptr;
    QLabel *m_cpuPeakLabel = nullptr;

    // Graphs panel
    QGroupBox *m_graphsGroup = nullptr;
    QVBoxLayout *m_graphsLayout = nullptr;
    QComboBox *m_graphTypeCombo = nullptr;
    QtCharts::QChartView *m_chartView = nullptr;
    QtCharts::QLineSeries *m_fpsSeries = nullptr;
    QtCharts::QLineSeries *m_frameTimeSeries = nullptr;
    QtCharts::QLineSeries *m_memorySeries = nullptr;
    QtCharts::QLineSeries *m_cpuSeries = nullptr;
    QtCharts::QLineSeries *m_gpuSeries = nullptr;

    // Details panel
    QGroupBox *m_detailsGroup = nullptr;
    QTableWidget *m_detailsTable = nullptr;

    // Performance data
    std::deque<PerformanceSample> m_samples;
    ProfilerStats m_stats;
    std::chrono::steady_clock::time_point m_startTime;

    // Settings
    bool m_isProfiling = false;
    bool m_isPaused = false;
    ProfilerMode m_profilerMode = ProfilerMode::RealTime;
    GraphType m_currentGraphType = GraphType::FPS;
    int m_sampleRate = 30; // Hz
    int m_historySizeSeconds = 60; // seconds
    int m_maxSamples = 1800; // 30Hz * 60 seconds

    // Update timer
    QTimer m_updateTimer;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
};

//===============================================================================
// Performance Monitor (singleton for data collection)
//===============================================================================

class PerformanceMonitor : public QObject
{
    Q_OBJECT

public:
    static PerformanceMonitor* instance();

    // Data collection
    void startCollection();
    void stopCollection();
    void pauseCollection();
    void resumeCollection();

    void recordSample(const PerformanceSample& sample);

    // Data access
    const std::deque<PerformanceSample>& samples() const { return m_samples; }
    const ProfilerStats& stats() const { return m_stats; }

    // Settings
    void setSampleRate(int hz) { m_sampleRate = hz; }
    void setHistorySize(int seconds) { m_historySizeSeconds = seconds; }

signals:
    void sampleRecorded(const PerformanceSample& sample);
    void statsUpdated(const ProfilerStats& stats);

private:
    PerformanceMonitor(QObject *parent = nullptr);
    ~PerformanceMonitor() override;

    void updateStats();
    void pruneOldData();

    static PerformanceMonitor* s_instance;

    std::deque<PerformanceSample> m_samples;
    ProfilerStats m_stats;
    bool m_isCollecting = false;
    bool m_isPaused = false;
    int m_sampleRate = 30;
    int m_historySizeSeconds = 60;
    int m_maxSamples = 1800;
};

//===============================================================================
// GPU Profiler (for GPU-specific metrics)
//===============================================================================

class GPUProfiler : public QObject
{
    Q_OBJECT

public:
    GPUProfiler(QObject *parent = nullptr);
    ~GPUProfiler() override;

    // GPU monitoring
    void startMonitoring();
    void stopMonitoring();

    // Metrics
    float getGPUUsage() const;
    uint64_t getGPUMemoryUsed() const;
    uint64_t getGPUMemoryTotal() const;
    float getGPUTemperature() const;

    // Frame timing
    void beginFrame();
    void endFrame();
    float getLastFrameTime() const; // milliseconds

signals:
    void gpuMetricsUpdated(float usage, uint64_t memoryUsed, uint64_t memoryTotal);

private:
    bool m_isMonitoring = false;
    float m_gpuUsage = 0.0f;
    uint64_t m_gpuMemoryUsed = 0;
    uint64_t m_gpuMemoryTotal = 0;
    float m_gpuTemperature = 0.0f;

    std::chrono::steady_clock::time_point m_frameStartTime;
    float m_lastFrameTime = 0.0f;
};

//===============================================================================
// Memory Profiler
//===============================================================================

class MemoryProfiler : public QObject
{
    Q_OBJECT

public:
    MemoryProfiler(QObject *parent = nullptr);
    ~MemoryProfiler() override;

    // Memory tracking
    void startTracking();
    void stopTracking();

    // Allocation tracking
    void trackAllocation(void* ptr, size_t size, const char* file, int line);
    void trackDeallocation(void* ptr);

    // Statistics
    struct MemoryStats {
        uint64_t totalAllocated = 0;
        uint64_t totalFreed = 0;
        uint64_t currentUsage = 0;
        uint64_t peakUsage = 0;
        uint32_t allocationCount = 0;
        uint32_t deallocationCount = 0;
        uint32_t liveAllocations = 0;
    };

    const MemoryStats& stats() const { return m_stats; }

signals:
    void memoryStatsUpdated(const MemoryStats& stats);

private:
    struct AllocationInfo {
        size_t size;
        const char* file;
        int line;
        std::chrono::steady_clock::time_point timestamp;
    };

    std::unordered_map<void*, AllocationInfo> m_allocations;
    MemoryStats m_stats;
    bool m_isTracking = false;
};