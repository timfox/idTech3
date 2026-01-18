/*
===============================================================================

Qt Profiler Widget Implementation

===============================================================================
*/

#include "profiler_widget.h"
#include "../common/qcommon.h"

#include <QApplication>
#include <QHeaderView>
#include <QDateTime>
#include <QFileDialog>
#include <QStandardPaths>
#include <QPixmap>
#include <QPainter>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <algorithm>
#include <numeric>

//===============================================================================
// ProfilerWidget
//===============================================================================

ProfilerWidget::ProfilerWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();

    // Set up update timer
    m_updateTimer.setInterval(1000 / 30); // 30 FPS
    connect(&m_updateTimer, &QTimer::timeout, this, &ProfilerWidget::updateDisplay);

    // Connect to performance monitor
    connect(PerformanceMonitor::instance(), &PerformanceMonitor::sampleRecorded,
            this, [this](const PerformanceSample& sample) {
                addPerformanceSample(sample);
            });
    connect(PerformanceMonitor::instance(), &PerformanceMonitor::statsUpdated,
            this, [this](const ProfilerStats& stats) {
                m_stats = stats;
                updateStatsDisplay();
            });
}

ProfilerWidget::~ProfilerWidget()
{
    stopProfiling();
}

void ProfilerWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    m_mainLayout->addWidget(m_mainSplitter);

    // Top section - toolbar and stats
    QWidget *topWidget = new QWidget();
    QVBoxLayout *topLayout = new QVBoxLayout(topWidget);

    createToolbar();
    topLayout->addLayout(m_toolbarLayout);

    createStatsPanel();
    topLayout->addWidget(m_statsGroup);

    m_mainSplitter->addWidget(topWidget);

    // Bottom section - graphs and details
    QWidget *bottomWidget = new QWidget();
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomWidget);

    createGraphsPanel();
    bottomLayout->addWidget(m_graphsGroup, 2);

    createDetailsPanel();
    bottomLayout->addWidget(m_detailsGroup, 1);

    m_mainSplitter->addWidget(bottomWidget);

    // Set splitter proportions
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 2);
}

void ProfilerWidget::createToolbar()
{
    m_toolbarLayout = new QHBoxLayout();

    m_startStopButton = new QPushButton("Start Profiling", this);
    m_startStopButton->setCheckable(true);
    m_toolbarLayout->addWidget(m_startStopButton);

    m_pauseResumeButton = new QPushButton("Pause", this);
    m_pauseResumeButton->setCheckable(true);
    m_pauseResumeButton->setEnabled(false);
    m_toolbarLayout->addWidget(m_pauseResumeButton);

    m_clearButton = new QPushButton("Clear", this);
    m_toolbarLayout->addWidget(m_clearButton);

    m_exportButton = new QPushButton("Export", this);
    m_toolbarLayout->addWidget(m_exportButton);

    m_toolbarLayout->addStretch();

    // Settings
    QLabel *modeLabel = new QLabel("Mode:", this);
    m_toolbarLayout->addWidget(modeLabel);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem("Real-time");
    m_modeCombo->addItem("Capture");
    m_modeCombo->addItem("Analysis");
    m_toolbarLayout->addWidget(m_modeCombo);

    QLabel *sampleLabel = new QLabel("Sample Rate:", this);
    m_toolbarLayout->addWidget(sampleLabel);

    m_sampleRateSpin = new QSpinBox(this);
    m_sampleRateSpin->setRange(1, 100);
    m_sampleRateSpin->setValue(m_sampleRate);
    m_sampleRateSpin->setSuffix(" Hz");
    m_toolbarLayout->addWidget(m_sampleRateSpin);

    QLabel *historyLabel = new QLabel("History:", this);
    m_toolbarLayout->addWidget(historyLabel);

    m_historySizeSpin = new QSpinBox(this);
    m_historySizeSpin->setRange(10, 300);
    m_historySizeSpin->setValue(m_historySizeSeconds);
    m_historySizeSpin->setSuffix(" sec");
    m_toolbarLayout->addWidget(m_historySizeSpin);
}

void ProfilerWidget::createStatsPanel()
{
    m_statsGroup = new QGroupBox("Performance Statistics", this);
    m_statsLayout = new QGridLayout(m_statsGroup);

    // Current values
    m_statsLayout->addWidget(new QLabel("FPS:", this), 0, 0);
    m_fpsLabel = new QLabel("0.0", this);
    m_fpsLabel->setStyleSheet("font-weight: bold; color: #00AA00;");
    m_statsLayout->addWidget(m_fpsLabel, 0, 1);

    m_statsLayout->addWidget(new QLabel("Frame Time:", this), 0, 2);
    m_frameTimeLabel = new QLabel("0.0 ms", this);
    m_frameTimeLabel->setStyleSheet("font-weight: bold;");
    m_statsLayout->addWidget(m_frameTimeLabel, 0, 3);

    m_statsLayout->addWidget(new QLabel("Memory:", this), 1, 0);
    m_memoryLabel = new QLabel("0 MB", this);
    m_memoryLabel->setStyleSheet("font-weight: bold; color: #0000AA;");
    m_statsLayout->addWidget(m_memoryLabel, 1, 1);

    m_statsLayout->addWidget(new QLabel("CPU:", this), 1, 2);
    m_cpuLabel = new QLabel("0.0%", this);
    m_cpuLabel->setStyleSheet("font-weight: bold; color: #AA0000;");
    m_statsLayout->addWidget(m_cpuLabel, 1, 3);

    m_statsLayout->addWidget(new QLabel("GPU:", this), 2, 0);
    m_gpuLabel = new QLabel("0.0%", this);
    m_gpuLabel->setStyleSheet("font-weight: bold; color: #AA00AA;");
    m_statsLayout->addWidget(m_gpuLabel, 2, 1);

    m_statsLayout->addWidget(new QLabel("Triangles:", this), 2, 2);
    m_trianglesLabel = new QLabel("0", this);
    m_statsLayout->addWidget(m_trianglesLabel, 2, 3);

    m_statsLayout->addWidget(new QLabel("Draw Calls:", this), 3, 0);
    m_drawCallsLabel = new QLabel("0", this);
    m_statsLayout->addWidget(m_drawCallsLabel, 3, 1);

    // Min/Max/Avg values
    m_fpsMinMaxLabel = new QLabel("Min: 0.0 | Max: 0.0 | Avg: 0.0", this);
    m_statsLayout->addWidget(m_fpsMinMaxLabel, 4, 0, 1, 4);

    m_memoryPeakLabel = new QLabel("Peak Memory: 0 MB", this);
    m_statsLayout->addWidget(m_memoryPeakLabel, 5, 0, 1, 2);

    m_cpuPeakLabel = new QLabel("Peak CPU: 0.0%", this);
    m_statsLayout->addWidget(m_cpuPeakLabel, 5, 2, 1, 2);
}

void ProfilerWidget::createGraphsPanel()
{
    m_graphsGroup = new QGroupBox("Performance Graphs", this);
    m_graphsLayout = new QVBoxLayout(m_graphsGroup);

    // Graph type selector
    QHBoxLayout *graphTypeLayout = new QHBoxLayout();
    graphTypeLayout->addWidget(new QLabel("Graph:", this));

    m_graphTypeCombo = new QComboBox(this);
    m_graphTypeCombo->addItem("FPS");
    m_graphTypeCombo->addItem("Frame Time");
    m_graphTypeCombo->addItem("Memory");
    m_graphTypeCombo->addItem("CPU Usage");
    m_graphTypeCombo->addItem("GPU Usage");
    m_graphTypeCombo->addItem("Triangles");
    m_graphTypeCombo->addItem("Draw Calls");
    graphTypeLayout->addWidget(m_graphTypeCombo);

    graphTypeLayout->addStretch();
    m_graphsLayout->addLayout(graphTypeLayout);

    // Chart view
    using namespace QtCharts;

    QChart *chart = new QChart();
    chart->setTitle("Performance Graph");
    chart->setAnimationOptions(QChart::NoAnimation);

    m_fpsSeries = new QLineSeries();
    m_fpsSeries->setName("FPS");
    chart->addSeries(m_fpsSeries);

    m_frameTimeSeries = new QLineSeries();
    m_frameTimeSeries->setName("Frame Time");
    chart->addSeries(m_frameTimeSeries);

    m_memorySeries = new QLineSeries();
    m_memorySeries->setName("Memory");
    chart->addSeries(m_memorySeries);

    m_cpuSeries = new QLineSeries();
    m_cpuSeries->setName("CPU");
    chart->addSeries(m_cpuSeries);

    m_gpuSeries = new QLineSeries();
    m_gpuSeries->setName("GPU");
    chart->addSeries(m_gpuSeries);

    // Initially show only FPS series
    m_frameTimeSeries->setVisible(false);
    m_memorySeries->setVisible(false);
    m_cpuSeries->setVisible(false);
    m_gpuSeries->setVisible(false);

    QValueAxis *axisX = new QValueAxis();
    axisX->setTitleText("Time (seconds)");
    axisX->setRange(0, m_historySizeSeconds);
    chart->addAxis(axisX, Qt::AlignBottom);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Value");
    axisY->setRange(0, 120); // FPS range
    chart->addAxis(axisY, Qt::AlignLeft);

    m_fpsSeries->attachAxis(axisX);
    m_fpsSeries->attachAxis(axisY);

    m_chartView = new QChartView(chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_graphsLayout->addWidget(m_chartView);
}

void ProfilerWidget::createDetailsPanel()
{
    m_detailsGroup = new QGroupBox("Sample Details", this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(m_detailsGroup);

    m_detailsTable = new QTableWidget(this);
    m_detailsTable->setColumnCount(8);
    m_detailsTable->setHorizontalHeaderLabels({
        "Time", "FPS", "Frame Time", "Memory", "CPU", "GPU", "Triangles", "Draw Calls"
    });

    m_detailsTable->horizontalHeader()->setStretchLastSection(true);
    m_detailsTable->verticalHeader()->setVisible(false);
    m_detailsTable->setAlternatingRowColors(true);
    m_detailsTable->setMaximumWidth(600);

    detailsLayout->addWidget(m_detailsTable);
}

void ProfilerWidget::setupConnections()
{
    connect(m_startStopButton, &QPushButton::toggled, this, &ProfilerWidget::onStartStopClicked);
    connect(m_pauseResumeButton, &QPushButton::toggled, this, &ProfilerWidget::onPauseResumeClicked);
    connect(m_clearButton, &QPushButton::clicked, this, &ProfilerWidget::onClearClicked);
    connect(m_exportButton, &QPushButton::clicked, this, &ProfilerWidget::onExportClicked);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilerWidget::onProfilerModeChanged);
    connect(m_sampleRateSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ProfilerWidget::onSampleRateChanged);
    connect(m_historySizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ProfilerWidget::onHistorySizeChanged);
    connect(m_graphTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProfilerWidget::onGraphTypeChanged);
}

void ProfilerWidget::startProfiling()
{
    if (m_isProfiling) return;

    m_isProfiling = true;
    m_startTime = std::chrono::steady_clock::now();

    PerformanceMonitor::instance()->startCollection();

    m_startStopButton->setChecked(true);
    m_startStopButton->setText("Stop Profiling");
    m_pauseResumeButton->setEnabled(true);

    m_updateTimer.start();

    emit profilingStarted();
}

void ProfilerWidget::stopProfiling()
{
    if (!m_isProfiling) return;

    m_isProfiling = false;
    m_isPaused = false;

    PerformanceMonitor::instance()->stopCollection();

    m_startStopButton->setChecked(false);
    m_startStopButton->setText("Start Profiling");
    m_pauseResumeButton->setChecked(false);
    m_pauseResumeButton->setText("Pause");
    m_pauseResumeButton->setEnabled(false);

    m_updateTimer.stop();

    emit profilingStopped();
}

void ProfilerWidget::pauseProfiling()
{
    if (!m_isProfiling || m_isPaused) return;

    m_isPaused = true;
    PerformanceMonitor::instance()->pauseCollection();

    m_pauseResumeButton->setChecked(true);
    m_pauseResumeButton->setText("Resume");
}

void ProfilerWidget::resumeProfiling()
{
    if (!m_isProfiling || !m_isPaused) return;

    m_isPaused = false;
    PerformanceMonitor::instance()->resumeCollection();

    m_pauseResumeButton->setChecked(false);
    m_pauseResumeButton->setText("Pause");
}

void ProfilerWidget::clearData()
{
    m_samples.clear();
    m_stats = ProfilerStats{};

    updateGraphs();
    updateDetailsTable();
    updateStatsDisplay();

    PerformanceMonitor::instance()->statsUpdated(m_stats);
}

void ProfilerWidget::addPerformanceSample(const PerformanceSample& sample)
{
    m_samples.push_back(sample);

    // Prune old data
    pruneOldData();

    // Update stats
    updateStats();

    emit dataUpdated();
}

void ProfilerWidget::updateDisplay()
{
    if (!m_isProfiling) return;

    updateGraphs();
    updateDetailsTable();
}

void ProfilerWidget::updateStats()
{
    if (m_samples.empty()) {
        m_stats = ProfilerStats{};
        return;
    }

    // Calculate statistics
    m_stats.totalSamples = m_samples.size();

    // FPS stats
    auto fpsValues = std::vector<float>();
    fpsValues.reserve(m_samples.size());
    for (const auto& sample : m_samples) {
        fpsValues.push_back(sample.fps);
    }

    if (!fpsValues.empty()) {
        m_stats.averageFPS = std::accumulate(fpsValues.begin(), fpsValues.end(), 0.0f) / fpsValues.size();
        m_stats.minFPS = *std::min_element(fpsValues.begin(), fpsValues.end());
        m_stats.maxFPS = *std::max_element(fpsValues.begin(), fpsValues.end());
    }

    // Frame time stats
    auto frameTimeValues = std::vector<float>();
    frameTimeValues.reserve(m_samples.size());
    for (const auto& sample : m_samples) {
        frameTimeValues.push_back(sample.frameTime);
    }

    if (!frameTimeValues.empty()) {
        m_stats.averageFrameTime = std::accumulate(frameTimeValues.begin(), frameTimeValues.end(), 0.0f) / frameTimeValues.size();
    }

    // Peak values
    for (const auto& sample : m_samples) {
        m_stats.totalMemoryPeak = std::max(m_stats.totalMemoryPeak, sample.memoryUsed);
        m_stats.cpuUsagePeak = std::max(m_stats.cpuUsagePeak, sample.cpuUsage);
        m_stats.gpuUsagePeak = std::max(m_stats.gpuUsagePeak, sample.gpuUsage);
    }

    PerformanceMonitor::instance()->statsUpdated(m_stats);
}

void ProfilerWidget::pruneOldData()
{
    if (m_samples.empty()) return;

    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - std::chrono::seconds(m_historySizeSeconds);

    while (!m_samples.empty() && m_samples.front().timestamp < cutoff) {
        m_samples.pop_front();
    }

    // Also limit by sample count
    while (m_samples.size() > m_maxSamples) {
        m_samples.pop_front();
    }
}

void ProfilerWidget::updateStatsDisplay()
{
    if (m_samples.empty()) {
        m_fpsLabel->setText("0.0");
        m_frameTimeLabel->setText("0.0 ms");
        m_memoryLabel->setText("0 MB");
        m_cpuLabel->setText("0.0%");
        m_gpuLabel->setText("0.0%");
        m_trianglesLabel->setText("0");
        m_drawCallsLabel->setText("0");
        return;
    }

    const auto& latest = m_samples.back();

    m_fpsLabel->setText(QString::number(latest.fps, 'f', 1));
    m_frameTimeLabel->setText(formatTime(latest.frameTime));
    m_memoryLabel->setText(formatMemory(latest.memoryUsed));
    m_cpuLabel->setText(formatPercentage(latest.cpuUsage));
    m_gpuLabel->setText(formatPercentage(latest.gpuUsage));
    m_trianglesLabel->setText(QString::number(latest.triangles));
    m_drawCallsLabel->setText(QString::number(latest.drawCalls));

    // Update min/max/avg display
    m_fpsMinMaxLabel->setText(QString("Min: %1 | Max: %2 | Avg: %3")
                             .arg(m_stats.minFPS, 0, 'f', 1)
                             .arg(m_stats.maxFPS, 0, 'f', 1)
                             .arg(m_stats.averageFPS, 0, 'f', 1));

    m_memoryPeakLabel->setText(QString("Peak Memory: %1").arg(formatMemory(m_stats.totalMemoryPeak)));
    m_cpuPeakLabel->setText(QString("Peak CPU: %1").arg(formatPercentage(m_stats.cpuUsagePeak)));
}

void ProfilerWidget::updateGraphs()
{
    if (m_samples.empty()) return;

    using namespace QtCharts;

    // Clear existing data
    m_fpsSeries->clear();
    m_frameTimeSeries->clear();
    m_memorySeries->clear();
    m_cpuSeries->clear();
    m_gpuSeries->clear();

    // Add data points
    for (size_t i = 0; i < m_samples.size(); ++i) {
        const auto& sample = m_samples[i];
        qreal timeOffset = i * (1.0 / m_sampleRate); // seconds

        m_fpsSeries->append(timeOffset, sample.fps);
        m_frameTimeSeries->append(timeOffset, sample.frameTime);
        m_memorySeries->append(timeOffset, sample.memoryUsed / (1024.0 * 1024.0)); // MB
        m_cpuSeries->append(timeOffset, sample.cpuUsage);
        m_gpuSeries->append(timeOffset, sample.gpuUsage);
    }

    // Update chart axes
    if (m_chartView && m_chartView->chart()) {
        QChart *chart = m_chartView->chart();

        // Update X axis range
        if (!m_samples.empty()) {
            qreal maxTime = m_samples.size() * (1.0 / m_sampleRate);
            for (auto *axis : chart->axes(Qt::Horizontal)) {
                if (auto *valueAxis = qobject_cast<QValueAxis*>(axis)) {
                    valueAxis->setRange(0, std::max(maxTime, qreal(m_historySizeSeconds)));
                }
            }
        }

        // Update Y axis range based on current graph type
        for (auto *axis : chart->axes(Qt::Vertical)) {
            if (auto *valueAxis = qobject_cast<QValueAxis*>(axis)) {
                switch (m_currentGraphType) {
                    case GraphType::FPS:
                        valueAxis->setRange(0, 120);
                        valueAxis->setTitleText("FPS");
                        break;
                    case GraphType::FrameTime:
                        valueAxis->setRange(0, 50);
                        valueAxis->setTitleText("Frame Time (ms)");
                        break;
                    case GraphType::Memory:
                        valueAxis->setRange(0, 1024); // MB
                        valueAxis->setTitleText("Memory (MB)");
                        break;
                    case GraphType::CPU:
                    case GraphType::GPU:
                        valueAxis->setRange(0, 100);
                        valueAxis->setTitleText("Usage (%)");
                        break;
                    default:
                        valueAxis->setRange(0, 100);
                        break;
                }
            }
        }
    }
}

void ProfilerWidget::updateDetailsTable()
{
    if (!m_detailsTable) return;

    m_detailsTable->setRowCount(m_samples.size());

    for (int row = 0; row < m_samples.size(); ++row) {
        const auto& sample = m_samples[row];

        m_detailsTable->setItem(row, 0, new QTableWidgetItem(QString::number(row * (1.0 / m_sampleRate), 'f', 2)));
        m_detailsTable->setItem(row, 1, new QTableWidgetItem(QString::number(sample.fps, 'f', 1)));
        m_detailsTable->setItem(row, 2, new QTableWidgetItem(formatTime(sample.frameTime)));
        m_detailsTable->setItem(row, 3, new QTableWidgetItem(formatMemory(sample.memoryUsed)));
        m_detailsTable->setItem(row, 4, new QTableWidgetItem(formatPercentage(sample.cpuUsage)));
        m_detailsTable->setItem(row, 5, new QTableWidgetItem(formatPercentage(sample.gpuUsage)));
        m_detailsTable->setItem(row, 6, new QTableWidgetItem(QString::number(sample.triangles)));
        m_detailsTable->setItem(row, 7, new QTableWidgetItem(QString::number(sample.drawCalls)));
    }

    m_detailsTable->resizeColumnsToContents();
}

void ProfilerWidget::setSampleRate(int hz)
{
    m_sampleRate = std::max(1, std::min(100, hz));
    m_maxSamples = m_sampleRate * m_historySizeSeconds;
    PerformanceMonitor::instance()->setSampleRate(m_sampleRate);
}

void ProfilerWidget::setHistorySize(int seconds)
{
    m_historySizeSeconds = std::max(10, std::min(300, seconds));
    m_maxSamples = m_sampleRate * m_historySizeSeconds;
    PerformanceMonitor::instance()->setHistorySize(m_historySizeSeconds);
}

void ProfilerWidget::setProfilerMode(ProfilerMode mode)
{
    m_profilerMode = mode;
    // TODO: Implement different profiler modes
}

void ProfilerWidget::exportData(const QString& filename)
{
    QJsonDocument doc;
    QJsonObject root;

    // Metadata
    root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["sampleRate"] = m_sampleRate;
    root["historySize"] = m_historySizeSeconds;
    root["totalSamples"] = (int)m_samples.size();

    // Statistics
    QJsonObject statsObj;
    statsObj["averageFPS"] = m_stats.averageFPS;
    statsObj["minFPS"] = m_stats.minFPS;
    statsObj["maxFPS"] = m_stats.maxFPS;
    statsObj["averageFrameTime"] = m_stats.averageFrameTime;
    statsObj["totalMemoryPeak"] = (double)m_stats.totalMemoryPeak;
    statsObj["cpuUsagePeak"] = m_stats.cpuUsagePeak;
    statsObj["gpuUsagePeak"] = m_stats.gpuUsagePeak;
    root["statistics"] = statsObj;

    // Samples
    QJsonArray samplesArray;
    for (const auto& sample : m_samples) {
        QJsonObject sampleObj;
        sampleObj["timestamp"] = QString::number(std::chrono::duration_cast<std::chrono::milliseconds>(
            sample.timestamp - m_startTime).count());
        sampleObj["fps"] = sample.fps;
        sampleObj["frameTime"] = sample.frameTime;
        sampleObj["memoryUsed"] = (double)sample.memoryUsed;
        sampleObj["cpuUsage"] = sample.cpuUsage;
        sampleObj["gpuUsage"] = sample.gpuUsage;
        sampleObj["triangles"] = (int)sample.triangles;
        sampleObj["drawCalls"] = (int)sample.drawCalls;
        samplesArray.append(sampleObj);
    }
    root["samples"] = samplesArray;

    doc.setObject(root);

    QFile file(filename);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void ProfilerWidget::exportScreenshot(const QString& filename)
{
    QPixmap pixmap(size());
    render(&pixmap);
    pixmap.save(filename);
}

QString ProfilerWidget::formatTime(float milliseconds) const
{
    return QString("%1 ms").arg(milliseconds, 0, 'f', 2);
}

QString ProfilerWidget::formatMemory(uint64_t bytes) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;

    while (size >= 1024 && unitIndex < 4) {
        size /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
}

QString ProfilerWidget::formatPercentage(float percentage) const
{
    return QString("%1%").arg(percentage, 0, 'f', 1);
}

QColor ProfilerWidget::getPerformanceColor(float value, float min, float max) const
{
    if (value <= min) return Qt::green;
    if (value >= max) return Qt::red;

    float ratio = (value - min) / (max - min);
    if (ratio < 0.5f) {
        return QColor::fromRgbF(2 * ratio, 1.0, 0.0); // Green to Yellow
    } else {
        return QColor::fromRgbF(1.0, 2 * (1 - ratio), 0.0); // Yellow to Red
    }
}

// Event handlers
void ProfilerWidget::onStartStopClicked()
{
    if (m_startStopButton->isChecked()) {
        startProfiling();
    } else {
        stopProfiling();
    }
}

void ProfilerWidget::onPauseResumeClicked()
{
    if (m_pauseResumeButton->isChecked()) {
        pauseProfiling();
    } else {
        resumeProfiling();
    }
}

void ProfilerWidget::onClearClicked()
{
    clearData();
}

void ProfilerWidget::onExportClicked()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Profiler Data",
                                                   "profiler_data.json",
                                                   "JSON files (*.json);;All files (*)");
    if (!filename.isEmpty()) {
        exportData(filename);
    }
}

void ProfilerWidget::onSampleRateChanged(int rate)
{
    setSampleRate(rate);
}

void ProfilerWidget::onHistorySizeChanged(int seconds)
{
    setHistorySize(seconds);
}

void ProfilerWidget::onGraphTypeChanged(int type)
{
    m_currentGraphType = static_cast<GraphType>(type);

    // Update series visibility
    m_fpsSeries->setVisible(type == 0);
    m_frameTimeSeries->setVisible(type == 1);
    m_memorySeries->setVisible(type == 2);
    m_cpuSeries->setVisible(type == 3);
    m_gpuSeries->setVisible(type == 4);

    updateGraphs();
}

void ProfilerWidget::onProfilerModeChanged(int mode)
{
    setProfilerMode(static_cast<ProfilerMode>(mode));
}

//===============================================================================
// PerformanceMonitor
//===============================================================================

PerformanceMonitor* PerformanceMonitor::s_instance = nullptr;

PerformanceMonitor* PerformanceMonitor::instance()
{
    if (!s_instance) {
        s_instance = new PerformanceMonitor();
    }
    return s_instance;
}

PerformanceMonitor::PerformanceMonitor(QObject *parent)
    : QObject(parent)
{
}

PerformanceMonitor::~PerformanceMonitor()
{
    s_instance = nullptr;
}

void PerformanceMonitor::startCollection()
{
    m_isCollecting = true;
    m_isPaused = false;
    m_samples.clear();
    m_stats = ProfilerStats{};
}

void PerformanceMonitor::stopCollection()
{
    m_isCollecting = false;
    m_isPaused = false;
}

void PerformanceMonitor::pauseCollection()
{
    m_isPaused = true;
}

void PerformanceMonitor::resumeCollection()
{
    m_isPaused = false;
}

void PerformanceMonitor::recordSample(const PerformanceSample& sample)
{
    if (!m_isCollecting || m_isPaused) return;

    m_samples.push_back(sample);
    pruneOldData();
    updateStats();

    emit sampleRecorded(sample);
}

void PerformanceMonitor::updateStats()
{
    // Similar to ProfilerWidget::updateStats()
    // Implementation would go here
}

void PerformanceMonitor::pruneOldData()
{
    if (m_samples.empty()) return;

    // Prune based on time and count limits
    auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds(m_historySizeSeconds);

    while (!m_samples.empty() && m_samples.front().timestamp < cutoff) {
        m_samples.pop_front();
    }

    while (m_samples.size() > m_maxSamples) {
        m_samples.pop_front();
    }
}

//===============================================================================
// GPUProfiler
//===============================================================================

GPUProfiler::GPUProfiler(QObject *parent)
    : QObject(parent)
{
}

GPUProfiler::~GPUProfiler()
{
    stopMonitoring();
}

void GPUProfiler::startMonitoring()
{
    m_isMonitoring = true;
    // TODO: Initialize GPU monitoring (OpenGL queries, Vulkan queries, etc.)
}

void GPUProfiler::stopMonitoring()
{
    m_isMonitoring = false;
    // TODO: Clean up GPU monitoring
}

float GPUProfiler::getGPUUsage() const
{
    // TODO: Implement GPU usage monitoring
    return m_gpuUsage;
}

uint64_t GPUProfiler::getGPUMemoryUsed() const
{
    // TODO: Implement GPU memory monitoring
    return m_gpuMemoryUsed;
}

uint64_t GPUProfiler::getGPUMemoryTotal() const
{
    // TODO: Implement GPU memory total query
    return m_gpuMemoryTotal;
}

float GPUProfiler::getGPUTemperature() const
{
    // TODO: Implement GPU temperature monitoring
    return m_gpuTemperature;
}

void GPUProfiler::beginFrame()
{
    if (!m_isMonitoring) return;
    m_frameStartTime = std::chrono::steady_clock::now();
}

void GPUProfiler::endFrame()
{
    if (!m_isMonitoring) return;

    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - m_frameStartTime);
    m_lastFrameTime = duration.count() / 1000.0f; // Convert to milliseconds

    // TODO: Collect GPU metrics and emit signal
    emit gpuMetricsUpdated(m_gpuUsage, m_gpuMemoryUsed, m_gpuMemoryTotal);
}

float GPUProfiler::getLastFrameTime() const
{
    return m_lastFrameTime;
}

//===============================================================================
// MemoryProfiler
//===============================================================================

MemoryProfiler::MemoryProfiler(QObject *parent)
    : QObject(parent)
{
}

MemoryProfiler::~MemoryProfiler()
{
    stopTracking();
}

void MemoryProfiler::startTracking()
{
    m_isTracking = true;
    m_allocations.clear();
    m_stats = MemoryStats{};
}

void MemoryProfiler::stopTracking()
{
    m_isTracking = false;
    m_allocations.clear();
}

void MemoryProfiler::trackAllocation(void* ptr, size_t size, const char* file, int line)
{
    if (!m_isTracking || !ptr) return;

    AllocationInfo info;
    info.size = size;
    info.file = file;
    info.line = line;
    info.timestamp = std::chrono::steady_clock::now();

    m_allocations[ptr] = info;

    m_stats.totalAllocated += size;
    m_stats.currentUsage += size;
    m_stats.peakUsage = std::max(m_stats.peakUsage, m_stats.currentUsage);
    m_stats.allocationCount++;
    m_stats.liveAllocations++;

    emit memoryStatsUpdated(m_stats);
}

void MemoryProfiler::trackDeallocation(void* ptr)
{
    if (!m_isTracking || !ptr) return;

    auto it = m_allocations.find(ptr);
    if (it != m_allocations.end()) {
        size_t size = it->second.size;
        m_allocations.erase(it);

        m_stats.totalFreed += size;
        m_stats.currentUsage -= size;
        m_stats.deallocationCount++;
        m_stats.liveAllocations--;

        emit memoryStatsUpdated(m_stats);
    }
}