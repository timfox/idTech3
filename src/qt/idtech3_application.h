/*
===============================================================================

Qt Integration for id Tech 3 - Main Application

Provides Qt-based UI integration with seamless game/engine interaction.

===============================================================================
*/

#pragma once

#include <QApplication>
#include <QObject>
#include <QTimer>
#include <QThread>
#include <QOpenGLWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTextEdit>
#include <QSplitter>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <functional>

class QOpenGLContext;
class QSurfaceFormat;

// Forward declarations
class IdTech3Window;
class GameRenderer;
class ConsoleWidget;
class MenuWidget;
class AssetBrowser;
class ProfilerWidget;

// Qt Application wrapper for id Tech 3
class IdTech3Application : public QApplication
{
    Q_OBJECT

public:
    IdTech3Application(int &argc, char **argv);
    ~IdTech3Application() override;

    // Initialize Qt integration
    bool initialize();

    // Get main window
    IdTech3Window* mainWindow() const { return m_mainWindow.get(); }

    // Game control
    void startGame();
    void pauseGame();
    void stopGame();
    bool isGameRunning() const { return m_gameRunning; }

    // UI mode switching
    void setUIMode(bool inGameUI);
    bool isInGameUI() const { return m_inGameUI; }

    // Console access
    void showConsole();
    void hideConsole();
    ConsoleWidget* console() const { return m_console.get(); }

    // Menu system
    void showMainMenu();
    void hideMainMenu();
    MenuWidget* menu() const { return m_menu.get(); }

    // Asset browser
    void showAssetBrowser();
    void hideAssetBrowser();
    AssetBrowser* assetBrowser() const { return m_assetBrowser.get(); }

    // Profiler
    void showProfiler();
    void hideProfiler();
    ProfilerWidget* profiler() const { return m_profiler.get(); }

signals:
    // Game engine signals
    void gameFrameRequested();        // Request next game frame
    void gameInitialized();           // Game engine initialized
    void gameShutdown();             // Game engine shutting down

    // UI signals
    void uiModeChanged(bool inGame);  // UI mode changed

public slots:
    // Game engine slots
    void onGameFrameCompleted();      // Game frame completed
    void onGameError(const QString& error); // Game error occurred

    // UI slots
    void onMenuActionTriggered(const QString& action);
    void onConsoleCommandEntered(const QString& command);

private:
    void setupApplication();
    void createMainWindow();
    void setupMenus();
    void setupDockWidgets();
    void connectSignals();

    // Application state
    bool m_initialized = false;
    bool m_gameRunning = false;
    bool m_inGameUI = false;

    // Main components
    std::unique_ptr<IdTech3Window> m_mainWindow;
    std::unique_ptr<GameRenderer> m_gameRenderer;
    std::unique_ptr<ConsoleWidget> m_console;
    std::unique_ptr<MenuWidget> m_menu;
    std::unique_ptr<AssetBrowser> m_assetBrowser;
    std::unique_ptr<ProfilerWidget> m_profiler;

    // Timers
    QTimer m_gameTimer;              // Game frame timer
    QTimer m_uiUpdateTimer;          // UI update timer

    // Threads
    QThread m_gameThread;            // Game engine thread (optional)

private slots:
    void updateGameFrame();
    void updateUI();
    void handleApplicationStateChange(Qt::ApplicationState state);
};

// Main window class
class IdTech3Window : public QMainWindow
{
    Q_OBJECT

public:
    IdTech3Window(QWidget *parent = nullptr);
    ~IdTech3Window() override;

    // Renderer access
    GameRenderer* renderer() const { return m_renderer.get(); }

    // UI management
    void setUIMode(bool inGameUI);
    bool isInGameUI() const { return m_inGameUI; }

    // Fullscreen toggle
    void toggleFullscreen();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupUI();
    void createMenus();
    void createStatusBar();
    void setupCentralWidget();

    bool m_inGameUI = false;
    std::unique_ptr<GameRenderer> m_renderer;
    QSplitter *m_mainSplitter = nullptr;

    // Menus
    QMenu *m_fileMenu = nullptr;
    QMenu *m_editMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_toolsMenu = nullptr;
    QMenu *m_helpMenu = nullptr;

    // Status bar
    QLabel *m_statusLabel = nullptr;
    QLabel *m_fpsLabel = nullptr;
    QLabel *m_memoryLabel = nullptr;

private slots:
    void onFullscreenToggled();
    void updateStatusBar();
};

// Game renderer widget (OpenGL integration)
class GameRenderer : public QOpenGLWidget
{
    Q_OBJECT

public:
    GameRenderer(QWidget *parent = nullptr);
    ~GameRenderer() override;

    // Game integration
    void setGameHwnd(void* hwnd);    // Set game window handle
    void* gameHwnd() const { return m_gameHwnd; }

    // Rendering control
    void setRenderingEnabled(bool enabled);
    bool isRenderingEnabled() const { return m_renderingEnabled; }

    // Performance
    void setTargetFPS(int fps);
    int targetFPS() const { return m_targetFPS; }

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    // Input handling
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    void setupOpenGLFormat();
    void initializeGameEngine();
    void shutdownGameEngine();
    void renderGameFrame();

    // Game engine integration
    void* m_gameHwnd = nullptr;
    bool m_renderingEnabled = true;
    bool m_gameInitialized = false;

    // Performance
    int m_targetFPS = 60;
    qint64 m_lastFrameTime = 0;

    // Input state
    bool m_mouseCaptured = false;
    QPoint m_lastMousePos;
};

// Global application instance
extern IdTech3Application* g_idTech3App;