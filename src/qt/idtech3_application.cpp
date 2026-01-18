/*
===============================================================================

Qt Integration for id Tech 3 - Main Application Implementation

===============================================================================
*/

#include "idtech3_application.h"
#include "console_widget.h"
#include "menu_widget.h"
#include "asset_browser.h"
#include "profiler_widget.h"

#include <QApplication>
#include <QMainWindow>
#include <QOpenGLWidget>
#include <QOpenGLContext>
#include <QSurfaceFormat>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QThread>
#include <QLabel>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QFontDialog>
#include <QColorDialog>

// Game engine includes
extern "C" {
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include "../common/qcommon.h"
}

// Global instance
IdTech3Application* g_idTech3App = nullptr;

//===============================================================================
// IdTech3Application
//===============================================================================

IdTech3Application::IdTech3Application(int &argc, char **argv)
    : QApplication(argc, argv)
{
    g_idTech3App = this;
    setupApplication();
}

IdTech3Application::~IdTech3Application()
{
    if (m_gameRunning) {
        stopGame();
    }

    g_idTech3App = nullptr;
}

void IdTech3Application::setupApplication()
{
    // Set application properties
    setApplicationName("id Tech 3");
    setApplicationVersion("1.0");
    setApplicationDisplayName("id Tech 3 - Enhanced Edition");
    setOrganizationName("id Software");
    setOrganizationDomain("idsoftware.com");

    // Set up OpenGL format for game rendering
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4); // MSAA
    QSurfaceFormat::setDefaultFormat(format);

    // Set high DPI scaling
    setAttribute(Qt::AA_EnableHighDpiScaling);
    setAttribute(Qt::AA_UseHighDpiPixmaps);

    // Connect application state changes
    connect(this, &QApplication::applicationStateChanged,
            this, &IdTech3Application::handleApplicationStateChange);
}

bool IdTech3Application::initialize()
{
    if (m_initialized) {
        return true;
    }

    try {
        createMainWindow();

        // Create UI components
        m_console = std::make_unique<ConsoleWidget>();
        m_menu = std::make_unique<MenuWidget>();
        m_assetBrowser = std::make_unique<AssetBrowser>();
        m_profiler = std::make_unique<ProfilerWidget>();

        setupMenus();
        setupDockWidgets();
        connectSignals();

        // Set up timers
        m_gameTimer.setInterval(1000 / 60); // 60 FPS
        connect(&m_gameTimer, &QTimer::timeout, this, &IdTech3Application::updateGameFrame);

        m_uiUpdateTimer.setInterval(1000 / 30); // 30 FPS UI updates
        connect(&m_uiUpdateTimer, &QTimer::timeout, this, &IdTech3Application::updateUI);

        m_initialized = true;
        return true;

    } catch (const std::exception& e) {
        QMessageBox::critical(nullptr, "Initialization Error",
                            QString("Failed to initialize Qt integration: %1").arg(e.what()));
        return false;
    }
}

void IdTech3Application::createMainWindow()
{
    m_mainWindow = std::make_unique<IdTech3Window>();
    m_mainWindow->setWindowTitle("id Tech 3 - Enhanced Edition");
    m_mainWindow->resize(1280, 720);
    m_mainWindow->show();
}

void IdTech3Application::setupMenus()
{
    if (!m_mainWindow) return;

    QMenuBar *menuBar = m_mainWindow->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    QAction *newGameAction = fileMenu->addAction("&New Game");
    QAction *loadGameAction = fileMenu->addAction("&Load Game...");
    QAction *saveGameAction = fileMenu->addAction("&Save Game...");
    fileMenu->addSeparator();
    QAction *settingsAction = fileMenu->addAction("&Settings...");
    QAction *quitAction = fileMenu->addAction("&Quit");

    connect(newGameAction, &QAction::triggered, this, [this]() {
        emit onMenuActionTriggered("new_game");
    });
    connect(loadGameAction, &QAction::triggered, this, [this]() {
        emit onMenuActionTriggered("load_game");
    });
    connect(saveGameAction, &QAction::triggered, this, [this]() {
        emit onMenuActionTriggered("save_game");
    });
    connect(settingsAction, &QAction::triggered, this, [this]() {
        emit onMenuActionTriggered("settings");
    });
    connect(quitAction, &QAction::triggered, this, &QApplication::quit);

    // Edit menu
    QMenu *editMenu = menuBar->addMenu("&Edit");
    QAction *undoAction = editMenu->addAction("&Undo");
    QAction *redoAction = editMenu->addAction("&Redo");
    editMenu->addSeparator();
    QAction *cutAction = editMenu->addAction("Cu&t");
    QAction *copyAction = editMenu->addAction("&Copy");
    QAction *pasteAction = editMenu->addAction("&Paste");

    // View menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    QAction *fullscreenAction = viewMenu->addAction("&Fullscreen");
    fullscreenAction->setCheckable(true);
    fullscreenAction->setShortcut(QKeySequence("F11"));
    QAction *consoleAction = viewMenu->addAction("&Console");
    consoleAction->setShortcut(QKeySequence("`"));
    QAction *assetBrowserAction = viewMenu->addAction("&Asset Browser");
    QAction *profilerAction = viewMenu->addAction("&Profiler");

    connect(fullscreenAction, &QAction::toggled, m_mainWindow.get(), &IdTech3Window::toggleFullscreen);
    connect(consoleAction, &QAction::triggered, this, &IdTech3Application::showConsole);
    connect(assetBrowserAction, &QAction::triggered, this, &IdTech3Application::showAssetBrowser);
    connect(profilerAction, &QAction::triggered, this, &IdTech3Application::showProfiler);

    // Tools menu
    QMenu *toolsMenu = menuBar->addMenu("&Tools");
    QAction *mapEditorAction = toolsMenu->addAction("&Map Editor");
    QAction *modelViewerAction = toolsMenu->addAction("&Model Viewer");
    QAction *shaderEditorAction = toolsMenu->addAction("&Shader Editor");
    QAction *benchmarkAction = toolsMenu->addAction("&Run Benchmark");

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    QAction *aboutAction = helpMenu->addAction("&About");
    QAction *manualAction = helpMenu->addAction("&Manual");
    QAction *reportBugAction = helpMenu->addAction("&Report Bug");

    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(m_mainWindow.get(), "About id Tech 3",
                         "id Tech 3 - Enhanced Edition\n\n"
                         "Modern Qt-based UI with classic gameplay\n"
                         "Enhanced graphics, physics, and modding support");
    });
}

void IdTech3Application::setupDockWidgets()
{
    if (!m_mainWindow) return;

    // Console dock widget
    QDockWidget *consoleDock = new QDockWidget("Console", m_mainWindow.get());
    consoleDock->setWidget(m_console.get());
    consoleDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, consoleDock);

    // Asset browser dock widget
    QDockWidget *assetDock = new QDockWidget("Asset Browser", m_mainWindow.get());
    assetDock->setWidget(m_assetBrowser.get());
    assetDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, assetDock);

    // Profiler dock widget
    QDockWidget *profilerDock = new QDockWidget("Profiler", m_mainWindow.get());
    profilerDock->setWidget(m_profiler.get());
    profilerDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, profilerDock);

    // Hide dock widgets by default
    consoleDock->hide();
    assetDock->hide();
    profilerDock->hide();
}

void IdTech3Application::connectSignals()
{
    // Connect console commands
    if (m_console) {
        connect(m_console.get(), &ConsoleWidget::commandEntered,
                this, &IdTech3Application::onConsoleCommandEntered);
    }

    // Connect menu actions
    if (m_menu) {
        connect(m_menu.get(), &MenuWidget::actionTriggered,
                this, &IdTech3Application::onMenuActionTriggered);
    }
}

void IdTech3Application::startGame()
{
    if (m_gameRunning) return;

    m_gameRunning = true;
    m_gameTimer.start();

    // Hide Qt UI, show game
    setUIMode(true);

    emit gameInitialized();
    Com_Printf("Qt: Game started\n");
}

void IdTech3Application::pauseGame()
{
    if (!m_gameRunning) return;

    m_gameTimer.stop();
    Com_Printf("Qt: Game paused\n");
}

void IdTech3Application::stopGame()
{
    if (!m_gameRunning) return;

    m_gameRunning = false;
    m_gameTimer.stop();

    // Show Qt UI
    setUIMode(false);

    emit gameShutdown();
    Com_Printf("Qt: Game stopped\n");
}

void IdTech3Application::setUIMode(bool inGameUI)
{
    if (m_inGameUI == inGameUI) return;

    m_inGameUI = inGameUI;

    if (m_mainWindow) {
        m_mainWindow->setUIMode(inGameUI);
    }

    emit uiModeChanged(inGameUI);
}

void IdTech3Application::showConsole()
{
    if (m_console && m_mainWindow) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_console->parent());
        if (dock) {
            dock->show();
            dock->raise();
        }
        m_console->setFocus();
    }
}

void IdTech3Application::hideConsole()
{
    if (m_console) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_console->parent());
        if (dock) {
            dock->hide();
        }
    }
}

void IdTech3Application::showMainMenu()
{
    if (m_menu) {
        m_menu->show();
        m_menu->raise();
    }
}

void IdTech3Application::hideMainMenu()
{
    if (m_menu) {
        m_menu->hide();
    }
}

void IdTech3Application::showAssetBrowser()
{
    if (m_assetBrowser && m_mainWindow) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_assetBrowser->parent());
        if (dock) {
            dock->show();
            dock->raise();
        }
    }
}

void IdTech3Application::hideAssetBrowser()
{
    if (m_assetBrowser) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_assetBrowser->parent());
        if (dock) {
            dock->hide();
        }
    }
}

void IdTech3Application::showProfiler()
{
    if (m_profiler && m_mainWindow) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_profiler->parent());
        if (dock) {
            dock->show();
            dock->raise();
        }
    }
}

void IdTech3Application::hideProfiler()
{
    if (m_profiler) {
        QDockWidget *dock = qobject_cast<QDockWidget*>(m_profiler->parent());
        if (dock) {
            dock->hide();
        }
    }
}

void IdTech3Application::updateGameFrame()
{
    if (!m_gameRunning) return;

    emit gameFrameRequested();

    // Update UI periodically
    static int uiUpdateCounter = 0;
    if (++uiUpdateCounter >= 2) { // Update UI every 2 frames
        uiUpdateCounter = 0;
        emit updateUI();
    }
}

void IdTech3Application::updateUI()
{
    // Update status bars, etc.
    if (m_mainWindow) {
        m_mainWindow->updateStatusBar();
    }
}

void IdTech3Application::onGameFrameCompleted()
{
    // Handle post-frame operations
}

void IdTech3Application::onGameError(const QString& error)
{
    QMessageBox::critical(m_mainWindow.get(), "Game Error", error);
}

void IdTech3Application::onMenuActionTriggered(const QString& action)
{
    if (action == "new_game") {
        startGame();
    } else if (action == "settings") {
        // Show settings dialog
    } else if (action == "quit") {
        stopGame();
        quit();
    }
}

void IdTech3Application::onConsoleCommandEntered(const QString& command)
{
    // Execute console command
    Cbuf_AddText(va("%s\n", command.toUtf8().constData()));
}

void IdTech3Application::handleApplicationStateChange(Qt::ApplicationState state)
{
    switch (state) {
        case Qt::ApplicationSuspended:
            pauseGame();
            break;
        case Qt::ApplicationActive:
            if (m_gameRunning) {
                // Resume game
            }
            break;
        default:
            break;
    }
}

//===============================================================================
// IdTech3Window
//===============================================================================

IdTech3Window::IdTech3Window(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();
}

IdTech3Window::~IdTech3Window()
{
}

void IdTech3Window::setupUI()
{
    setWindowIcon(QIcon(":/icons/idtech3.png"));
    setMinimumSize(800, 600);

    createMenus();
    createStatusBar();
    setupCentralWidget();
}

void IdTech3Window::createMenus()
{
    // Menus are created in IdTech3Application::setupMenus()
}

void IdTech3Window::createStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_fpsLabel = new QLabel("FPS: 0");
    m_memoryLabel = new QLabel("Memory: 0 MB");

    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_fpsLabel);
    statusBar()->addPermanentWidget(m_memoryLabel);
}

void IdTech3Window::setupCentralWidget()
{
    // Create central widget with splitter for game view and UI
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);

    // Game renderer widget
    m_renderer = std::make_unique<GameRenderer>(m_mainSplitter);
    m_mainSplitter->addWidget(m_renderer.get());

    // Set splitter proportions (80% game, 20% UI)
    m_mainSplitter->setStretchFactor(0, 4);
    m_mainSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_mainSplitter);
}

void IdTech3Window::setUIMode(bool inGameUI)
{
    m_inGameUI = inGameUI;

    if (inGameUI) {
        // Hide Qt chrome, maximize game view
        menuBar()->hide();
        statusBar()->hide();
        setWindowState(windowState() | Qt::WindowFullScreen);
    } else {
        // Show Qt chrome
        menuBar()->show();
        statusBar()->show();
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
}

void IdTech3Window::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void IdTech3Window::closeEvent(QCloseEvent *event)
{
    if (g_idTech3App && g_idTech3App->isGameRunning()) {
        int result = QMessageBox::question(this, "Quit Game",
                                         "A game is currently running. Are you sure you want to quit?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::No) {
            event->ignore();
            return;
        }
        g_idTech3App->stopGame();
    }

    QMainWindow::closeEvent(event);
}

void IdTech3Window::keyPressEvent(QKeyEvent *event)
{
    // Handle Qt-specific shortcuts first
    if (event->key() == Qt::Key_F11) {
        toggleFullscreen();
        return;
    }

    // Pass other keys to game renderer
    if (m_renderer) {
        m_renderer->keyPressEvent(event);
    }
}

void IdTech3Window::keyReleaseEvent(QKeyEvent *event)
{
    // Pass keys to game renderer
    if (m_renderer) {
        m_renderer->keyReleaseEvent(event);
    }
}

void IdTech3Window::onFullscreenToggled()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        if (action->isChecked()) {
            showFullScreen();
        } else {
            showNormal();
        }
    }
}

void IdTech3Window::updateStatusBar()
{
    if (m_fpsLabel) {
        // Update FPS from game
        static int lastFPS = 0;
        // lastFPS = getCurrentFPS(); // TODO: Get from game
        m_fpsLabel->setText(QString("FPS: %1").arg(lastFPS));
    }

    if (m_memoryLabel) {
        // Update memory usage
        static int lastMemory = 0;
        // lastMemory = getMemoryUsage(); // TODO: Get from system
        m_memoryLabel->setText(QString("Memory: %1 MB").arg(lastMemory));
    }
}

//===============================================================================
// GameRenderer
//===============================================================================

GameRenderer::GameRenderer(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setupOpenGLFormat();

    // Enable mouse tracking
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Accept focus to receive keyboard events
    setFocus();
}

GameRenderer::~GameRenderer()
{
    shutdownGameEngine();
}

void GameRenderer::setupOpenGLFormat()
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4); // MSAA
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    setFormat(format);
}

void GameRenderer::initializeGL()
{
    // Initialize OpenGL context
    initializeOpenGLFunctions();

    // Set clear color
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Initialize game engine
    initializeGameEngine();

    Com_Printf("Qt: OpenGL context initialized\n");
}

void GameRenderer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    // Notify game engine of resize
    if (m_gameInitialized) {
        // TODO: Call game resize function
    }
}

void GameRenderer::paintGL()
{
    if (!m_renderingEnabled) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    renderGameFrame();
}

void GameRenderer::renderGameFrame()
{
    if (!m_gameInitialized) {
        // Render loading screen or placeholder
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    // TODO: Call game rendering function
    // For now, just clear
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Calculate frame time
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 deltaTime = currentTime - m_lastFrameTime;
    m_lastFrameTime = currentTime;

    // Limit frame rate if needed
    if (m_targetFPS > 0) {
        int targetFrameTime = 1000 / m_targetFPS;
        if (deltaTime < targetFrameTime) {
            QThread::msleep(targetFrameTime - deltaTime);
        }
    }
}

void GameRenderer::initializeGameEngine()
{
    if (m_gameInitialized) return;

    // TODO: Initialize game engine with this OpenGL context
    // Pass the OpenGL context to the game engine

    m_gameInitialized = true;
    Com_Printf("Qt: Game engine initialized\n");
}

void GameRenderer::shutdownGameEngine()
{
    if (!m_gameInitialized) return;

    // TODO: Shutdown game engine

    m_gameInitialized = false;
    Com_Printf("Qt: Game engine shutdown\n");
}

void GameRenderer::setGameHwnd(void* hwnd)
{
    m_gameHwnd = hwnd;
}

void GameRenderer::setRenderingEnabled(bool enabled)
{
    m_renderingEnabled = enabled;
    update();
}

void GameRenderer::setTargetFPS(int fps)
{
    m_targetFPS = fps;
}

// Input event handlers - forward to game engine
void GameRenderer::mousePressEvent(QMouseEvent *event)
{
    if (m_gameInitialized) {
        // TODO: Forward to game input system
    }
    event->accept();
}

void GameRenderer::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_gameInitialized) {
        // TODO: Forward to game input system
    }
    event->accept();
}

void GameRenderer::mouseMoveEvent(QMouseEvent *event)
{
    if (m_gameInitialized && m_mouseCaptured) {
        // TODO: Forward to game input system
        // Convert Qt coordinates to game coordinates
        QPoint pos = event->pos();
        // IN_MouseMove(pos.x(), pos.y());
    }
    event->accept();
}

void GameRenderer::wheelEvent(QWheelEvent *event)
{
    if (m_gameInitialized) {
        // TODO: Forward mouse wheel to game
    }
    event->accept();
}

void GameRenderer::keyPressEvent(QKeyEvent *event)
{
    if (m_gameInitialized) {
        // TODO: Forward key press to game
        // Convert Qt key codes to game key codes
        int key = event->key();
        // IN_KeyDown(key);
    }
    event->accept();
}

void GameRenderer::keyReleaseEvent(QKeyEvent *event)
{
    if (m_gameInitialized) {
        // TODO: Forward key release to game
    }
    event->accept();
}