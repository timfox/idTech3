/*
===============================================================================

Qt Main Entry Point for id Tech 3

Demonstrates Qt integration with the id Tech 3 engine.

===============================================================================
*/

#include "idtech3_application.h"
#include "console_widget.h"
#include "menu_widget.h"
#include "asset_browser.h"
#include "profiler_widget.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QTranslator>
#include <QLocale>
#include <QTimer>
#include <QDebug>

// Game engine includes
extern "C" {
#include "../sys/sys_local.h"
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include "../common/qcommon.h"
#include "../common/q_shared.h"
}

// Qt application instance
IdTech3Application* qtApp = nullptr;

// Engine integration functions
extern "C" {

// Called from engine to initialize Qt
void Sys_InitQt(int argc, char* argv[]) {
    if (qtApp) return; // Already initialized

    try {
        // Create Qt application
        qtApp = new IdTech3Application(argc, argv);

        // Set application properties
        qtApp->setApplicationName("id Tech 3");
        qtApp->setApplicationDisplayName("id Tech 3 - Enhanced Edition");
        qtApp->setApplicationVersion("1.0");
        qtApp->setOrganizationName("id Software");

        // Load custom fonts
        QString fontDir = ":/fonts/";
        QStringList fontFiles = {"quake.ttf"};

        for (const QString& fontFile : fontFiles) {
            QString fontPath = fontDir + fontFile;
            if (QFontDatabase::addApplicationFont(fontPath) == -1) {
                Com_Printf("Qt: Failed to load font: %s\n", fontFile.toUtf8().constData());
            } else {
                Com_Printf("Qt: Loaded font: %s\n", fontFile.toUtf8().constData());
            }
        }

        // Set application style
        qtApp->setStyle(QStyleFactory::create("Fusion"));

        // Set dark theme palette
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(32, 32, 32));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(32, 32, 32));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(32, 32, 32));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qtApp->setPalette(darkPalette);

        // Load stylesheet
        QFile styleFile(":/styles/dark.qss");
        if (styleFile.open(QFile::ReadOnly)) {
            QString styleSheet = QLatin1String(styleFile.readAll());
            qtApp->setStyleSheet(styleSheet);
            styleFile.close();
        }

        // Initialize Qt integration
        if (!qtApp->initialize()) {
            Com_Error(ERR_FATAL, "Failed to initialize Qt integration\n");
            return;
        }

        Com_Printf("Qt: Integration initialized successfully\n");

    } catch (const std::exception& e) {
        Com_Error(ERR_FATAL, "Qt initialization failed: %s\n", e.what());
    }
}

// Called from engine to shutdown Qt
void Sys_ShutdownQt() {
    if (!qtApp) return;

    qtApp->stopGame();
    qtApp->quit();

    delete qtApp;
    qtApp = nullptr;

    Com_Printf("Qt: Integration shutdown\n");
}

// Called from engine main loop to process Qt events
void Sys_ProcessQtEvents() {
    if (!qtApp) return;

    // Process Qt events
    qtApp->processEvents();

    // Update game frame if running
    if (qtApp->isGameRunning()) {
        // Signal game frame request
        emit qtApp->gameFrameRequested();
    }
}

// Called from engine to show Qt UI
void Sys_ShowQtUI(qboolean show) {
    if (!qtApp) return;

    qtApp->setUIMode(show);
}

// Qt console integration
void Sys_QtConsolePrint(const char* text) {
    if (!qtApp || !qtApp->console()) return;

    qtApp->console()->print(text);
}

void Sys_QtConsoleExecute(const char* command) {
    if (!qtApp) return;

    emit qtApp->console()->commandEntered(command);
}

// Qt menu integration
void Sys_QtShowMainMenu() {
    if (!qtApp) return;

    qtApp->showMainMenu();
}

void Sys_QtHideMainMenu() {
    if (!qtApp) return;

    qtApp->hideMainMenu();
}

// Qt asset browser integration
void Sys_QtShowAssetBrowser() {
    if (!qtApp) return;

    qtApp->showAssetBrowser();
}

void Sys_QtHideAssetBrowser() {
    if (!qtApp) return;

    qtApp->hideAssetBrowser();
}

// Qt profiler integration
void Sys_QtShowProfiler() {
    if (!qtApp) return;

    qtApp->showProfiler();
}

void Sys_QtHideProfiler() {
    if (!qtApp) return;

    qtApp->hideProfiler();
}

// Qt game launcher
void Sys_QtLaunchGame(const char* mod, const char* map, const char* difficulty) {
    if (!qtApp) return;

    qtApp->stopGame(); // Stop current game if running

    // TODO: Set up game parameters
    Com_Printf("Qt: Launching game - Mod: %s, Map: %s, Difficulty: %s\n",
               mod ? mod : "baseq3", map ? map : "q3dm1", difficulty ? difficulty : "medium");

    qtApp->startGame();
}

// Performance monitoring integration
void Sys_QtRecordPerformanceSample(float fps, float frameTime, uint32_t triangles,
                                  uint32_t drawCalls, uint64_t memoryUsed,
                                  float cpuUsage, float gpuUsage) {
    if (!qtApp) return;

    // Create performance sample
    PerformanceSample sample;
    sample.timestamp = std::chrono::steady_clock::now();
    sample.fps = fps;
    sample.frameTime = frameTime;
    sample.triangles = triangles;
    sample.drawCalls = drawCalls;
    sample.memoryUsed = memoryUsed;
    sample.cpuUsage = cpuUsage;
    sample.gpuUsage = gpuUsage;

    // Record sample
    PerformanceMonitor::instance()->recordSample(sample);
}

// Error handling
void Sys_QtError(const char* error) {
    if (!qtApp) return;

    emit qtApp->gameError(QString(error));
}

// Get Qt application instance
IdTech3Application* Sys_GetQtApplication() {
    return qtApp;
}

} // extern "C"

// Qt main function (alternative entry point)
int main_qt(int argc, char* argv[]) {
    // Initialize Qt application
    IdTech3Application app(argc, argv);

    if (!app.initialize()) {
        QMessageBox::critical(nullptr, "Initialization Error",
                            "Failed to initialize Qt integration");
        return 1;
    }

    // Show main menu
    app.showMainMenu();

    // Start Qt event loop
    return app.exec();
}

// Test function for Qt integration
void Test_QtIntegration() {
    Com_Printf("Testing Qt integration...\n");

    if (!qtApp) {
        Com_Printf("Qt not initialized, initializing...\n");
        char* test_argv[] = {"idtech3", nullptr};
        Sys_InitQt(1, test_argv);
    }

    if (qtApp) {
        Com_Printf("Qt integration active\n");

        // Test console
        Sys_QtConsolePrint("Qt console test message\n");

        // Test menu
        Sys_QtShowMainMenu();

        // Test asset browser
        Sys_QtShowAssetBrowser();

        // Test profiler
        Sys_QtShowProfiler();

        Com_Printf("Qt integration test completed\n");
    } else {
        Com_Printf("Qt integration failed\n");
    }
}