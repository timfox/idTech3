/*
===============================================================================

Qt Launcher for id Tech 3

Standalone Qt application that provides a modern interface for launching
and managing id Tech 3 games.

===============================================================================
*/

#include "idtech3_application.h"
#include "menu_widget.h"
#include "console_widget.h"
#include "asset_browser.h"
#include "profiler_widget.h"

#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QSplitter>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QDir>
#include <QTimer>
#include <QPixmap>
#include <QMovie>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>

#include <memory>
#include <vector>

// Game engine interface (would be linked)
extern "C" {
    // Engine functions
    int main_engine(int argc, char* argv[]);
    void Sys_InitQt(int argc, char* argv[]);
    void Sys_ShutdownQt();
    void Sys_ProcessQtEvents();
}

// Launcher main window
class QtLauncher : public QMainWindow
{
    Q_OBJECT

public:
    QtLauncher(QWidget *parent = nullptr);
    ~QtLauncher() override;

    void initialize();
    void setGamePath(const QString& path);
    QString gamePath() const { return m_gamePath; }

public slots:
    void launchGame();
    void stopGame();
    void showSettings();
    void showAbout();
    void checkForUpdates();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onGameLaunched();
    void onGameFinished(int exitCode);
    void onBrowseGamePath();
    void onBrowseWorkingDir();
    void onSaveSettings();
    void onLoadSettings();

private:
    void setupUI();
    void createMenus();
    void createToolbar();
    void createCentralWidget();
    void createStatusBar();
    void loadSettings();
    void saveSettings();
    void updateUI();

    // Game process
    QProcess *m_gameProcess;
    bool m_gameRunning;

    // Settings
    QString m_gamePath;
    QString m_workingDirectory;
    QStringList m_commandLineArgs;
    QString m_modName;
    QString m_mapName;
    QString m_configFile;

    // UI components
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;

    // Game selection
    QGroupBox *m_gameGroup;
    QLineEdit *m_gamePathEdit;
    QPushButton *m_browseGameButton;
    QLineEdit *m_workingDirEdit;
    QPushButton *m_browseWorkButton;

    // Launch options
    QGroupBox *m_optionsGroup;
    QLineEdit *m_modEdit;
    QLineEdit *m_mapEdit;
    QLineEdit *m_argsEdit;
    QComboBox *m_configCombo;
    QCheckBox *m_windowedCheck;
    QCheckBox *m_developerCheck;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;

    // Launch button
    QPushButton *m_launchButton;
    QPushButton *m_stopButton;
    QProgressBar *m_progressBar;

    // Console output
    QGroupBox *m_consoleGroup;
    QTextEdit *m_consoleText;

    // Status bar
    QLabel *m_statusLabel;
    QLabel *m_versionLabel;
};

//===============================================================================
// QtLauncher implementation
//===============================================================================

QtLauncher::QtLauncher(QWidget *parent)
    : QMainWindow(parent)
    , m_gameProcess(nullptr)
    , m_gameRunning(false)
{
    setWindowTitle("id Tech 3 Launcher - Qt Edition");
    setMinimumSize(800, 600);
    resize(1000, 700);

    // Load settings
    loadSettings();

    // Setup UI
    setupUI();
    updateUI();

    // Set window icon
    setWindowIcon(QIcon(":/icons/idtech3.png"));
}

QtLauncher::~QtLauncher()
{
    saveSettings();
    stopGame();
}

void QtLauncher::initialize()
{
    // Initialize Qt integration
    char* argv[] = { "qt_launcher", nullptr };
    Sys_InitQt(1, argv);

    // Connect signals
    connect(m_launchButton, &QPushButton::clicked, this, &QtLauncher::launchGame);
    connect(m_stopButton, &QPushButton::clicked, this, &QtLauncher::stopGame);
    connect(m_browseGameButton, &QPushButton::clicked, this, &QtLauncher::onBrowseGamePath);
    connect(m_browseWorkButton, &QPushButton::clicked, this, &QtLauncher::onBrowseWorkingDir);
}

void QtLauncher::setupUI()
{
    createMenus();
    createToolbar();
    createCentralWidget();
    createStatusBar();
}

void QtLauncher::createMenus()
{
    QMenuBar *menuBar = this->menuBar();

    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    QAction *settingsAction = fileMenu->addAction("&Settings...");
    QAction *exitAction = fileMenu->addAction("E&xit");

    connect(settingsAction, &QAction::triggered, this, &QtLauncher::showSettings);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // Game menu
    QMenu *gameMenu = menuBar->addMenu("&Game");
    QAction *launchAction = gameMenu->addAction("&Launch Game");
    QAction *stopAction = gameMenu->addAction("&Stop Game");
    gameMenu->addSeparator();
    QAction *browseModsAction = gameMenu->addAction("&Browse Mods...");

    connect(launchAction, &QAction::triggered, this, &QtLauncher::launchGame);
    connect(stopAction, &QAction::triggered, this, &QtLauncher::stopGame);

    // Tools menu
    QMenu *toolsMenu = menuBar->addMenu("&Tools");
    QAction *consoleAction = toolsMenu->addAction("&Console");
    QAction *assetBrowserAction = toolsMenu->addAction("&Asset Browser");
    QAction *profilerAction = toolsMenu->addAction("&Profiler");

    connect(consoleAction, &QAction::triggered, []() {
        if (auto app = Sys_GetQtApplication()) {
            app->showConsole();
        }
    });
    connect(assetBrowserAction, &QAction::triggered, []() {
        if (auto app = Sys_GetQtApplication()) {
            app->showAssetBrowser();
        }
    });
    connect(profilerAction, &QAction::triggered, []() {
        if (auto app = Sys_GetQtApplication()) {
            app->showProfiler();
        }
    });

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    QAction *checkUpdatesAction = helpMenu->addAction("Check for &Updates");
    QAction *aboutAction = helpMenu->addAction("&About");

    connect(checkUpdatesAction, &QAction::triggered, this, &QtLauncher::checkForUpdates);
    connect(aboutAction, &QAction::triggered, this, &QtLauncher::showAbout);
}

void QtLauncher::createToolbar()
{
    // Simple toolbar with launch/stop buttons
    QToolBar *toolbar = addToolBar("Main");
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    QAction *launchAction = toolbar->addAction("Launch");
    QAction *stopAction = toolbar->addAction("Stop");

    connect(launchAction, &QAction::triggered, this, &QtLauncher::launchGame);
    connect(stopAction, &QAction::triggered, this, &QtLauncher::stopGame);
}

void QtLauncher::createCentralWidget()
{
    m_centralWidget = new QWidget;
    setCentralWidget(m_centralWidget);

    m_mainLayout = new QVBoxLayout(m_centralWidget);

    // Game selection group
    m_gameGroup = new QGroupBox("Game Configuration");
    QFormLayout *gameLayout = new QFormLayout(m_gameGroup);

    QHBoxLayout *gamePathLayout = new QHBoxLayout;
    m_gamePathEdit = new QLineEdit(m_gamePath);
    m_browseGameButton = new QPushButton("Browse...");
    gamePathLayout->addWidget(m_gamePathEdit);
    gamePathLayout->addWidget(m_browseGameButton);
    gameLayout->addRow("Game Executable:", gamePathLayout);

    QHBoxLayout *workDirLayout = new QHBoxLayout;
    m_workingDirEdit = new QLineEdit(m_workingDirectory);
    m_browseWorkButton = new QPushButton("Browse...");
    workDirLayout->addWidget(m_workingDirEdit);
    workDirLayout->addWidget(m_browseWorkButton);
    gameLayout->addRow("Working Directory:", workDirLayout);

    m_mainLayout->addWidget(m_gameGroup);

    // Options group
    m_optionsGroup = new QGroupBox("Launch Options");
    QFormLayout *optionsLayout = new QFormLayout(m_optionsGroup);

    m_modEdit = new QLineEdit("baseq3");
    optionsLayout->addRow("Mod:", m_modEdit);

    m_mapEdit = new QLineEdit("q3dm1");
    optionsLayout->addRow("Map:", m_mapEdit);

    m_argsEdit = new QLineEdit("+set fs_game baseq3");
    optionsLayout->addRow("Additional Args:", m_argsEdit);

    m_configCombo = new QComboBox;
    m_configCombo->addItem("Default");
    m_configCombo->addItem("High Quality");
    m_configCombo->addItem("Low Quality");
    m_configCombo->addItem("Custom");
    optionsLayout->addRow("Configuration:", m_configCombo);

    QHBoxLayout *videoLayout = new QHBoxLayout;
    m_widthSpin = new QSpinBox;
    m_widthSpin->setRange(640, 7680);
    m_widthSpin->setValue(1280);
    m_heightSpin = new QSpinBox;
    m_heightSpin->setRange(480, 4320);
    m_heightSpin->setValue(720);
    videoLayout->addWidget(new QLabel("Resolution:"));
    videoLayout->addWidget(m_widthSpin);
    videoLayout->addWidget(new QLabel("x"));
    videoLayout->addWidget(m_heightSpin);
    videoLayout->addStretch();
    optionsLayout->addRow(videoLayout);

    QHBoxLayout *checkLayout = new QHBoxLayout;
    m_windowedCheck = new QCheckBox("Windowed Mode");
    m_windowedCheck->setChecked(true);
    m_developerCheck = new QCheckBox("Developer Mode");
    checkLayout->addWidget(m_windowedCheck);
    checkLayout->addWidget(m_developerCheck);
    checkLayout->addStretch();
    optionsLayout->addRow(checkLayout);

    m_mainLayout->addWidget(m_optionsGroup);

    // Launch controls
    QHBoxLayout *launchLayout = new QHBoxLayout;
    m_launchButton = new QPushButton("Launch Game");
    m_launchButton->setMinimumHeight(40);
    m_launchButton->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; "
        "border: none; border-radius: 5px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #45a049; }"
        "QPushButton:pressed { background-color: #3d8b40; }"
    );

    m_stopButton = new QPushButton("Stop Game");
    m_stopButton->setEnabled(false);
    m_stopButton->setMinimumHeight(40);
    m_stopButton->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; "
        "border: none; border-radius: 5px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #da190b; }"
        "QPushButton:pressed { background-color: #b71c1c; }"
    );

    launchLayout->addStretch();
    launchLayout->addWidget(m_stopButton);
    launchLayout->addWidget(m_launchButton);
    launchLayout->addStretch();

    m_mainLayout->addLayout(launchLayout);

    // Progress bar
    m_progressBar = new QProgressBar;
    m_progressBar->setRange(0, 0); // Indeterminate
    m_progressBar->setVisible(false);
    m_mainLayout->addWidget(m_progressBar);

    // Console output
    m_consoleGroup = new QGroupBox("Console Output");
    QVBoxLayout *consoleLayout = new QVBoxLayout(m_consoleGroup);

    m_consoleText = new QTextEdit;
    m_consoleText->setReadOnly(true);
    m_consoleText->setMaximumHeight(200);
    m_consoleText->setFont(QFont("Courier New", 9));
    consoleLayout->addWidget(m_consoleText);

    m_mainLayout->addWidget(m_consoleGroup);
}

void QtLauncher::createStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    statusBar()->addWidget(m_statusLabel);

    statusBar()->addPermanentWidget(new QLabel("Qt Edition"));
    m_versionLabel = new QLabel("v1.0");
    statusBar()->addPermanentWidget(m_versionLabel);
}

void QtLauncher::launchGame()
{
    if (m_gameRunning) return;

    QString gamePath = m_gamePathEdit->text();
    if (gamePath.isEmpty() || !QFile::exists(gamePath)) {
        QMessageBox::warning(this, "Launch Error",
                           "Please specify a valid game executable path.");
        return;
    }

    // Build command line arguments
    QStringList arguments;
    arguments << "+set" << "fs_game" << m_modEdit->text();
    arguments << "+map" << m_mapEdit->text();

    if (m_windowedCheck->isChecked()) {
        arguments << "+set" << "r_fullscreen" << "0";
    }

    if (m_developerCheck->isChecked()) {
        arguments << "+set" << "developer" << "1";
    }

    arguments << "+set" << "r_width" << QString::number(m_widthSpin->value());
    arguments << "+set" << "r_height" << QString::number(m_heightSpin->value());

    // Add additional arguments
    QStringList extraArgs = m_argsEdit->text().split(" ", Qt::SkipEmptyParts);
    arguments << extraArgs;

    // Start game process
    m_gameProcess = new QProcess(this);
    m_gameProcess->setProgram(gamePath);
    m_gameProcess->setArguments(arguments);
    m_gameProcess->setWorkingDirectory(m_workingDirEdit->text());

    connect(m_gameProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &QtLauncher::onGameFinished);
    connect(m_gameProcess, &QProcess::readyReadStandardOutput,
            [this]() {
                QByteArray output = m_gameProcess->readAllStandardOutput();
                m_consoleText->append(QString::fromUtf8(output));
            });
    connect(m_gameProcess, &QProcess::readyReadStandardError,
            [this]() {
                QByteArray error = m_gameProcess->readAllStandardError();
                m_consoleText->append(QString::fromUtf8(error));
            });

    m_consoleText->clear();
    m_progressBar->setVisible(true);
    m_statusLabel->setText("Launching game...");

    m_gameProcess->start();
    if (m_gameProcess->waitForStarted(5000)) {
        m_gameRunning = true;
        m_launchButton->setEnabled(false);
        m_stopButton->setEnabled(true);
        m_statusLabel->setText("Game running");
        m_progressBar->setVisible(false);
        emit onGameLaunched();
    } else {
        QMessageBox::critical(this, "Launch Error",
                            "Failed to start game process:\n" + m_gameProcess->errorString());
        m_progressBar->setVisible(false);
        delete m_gameProcess;
        m_gameProcess = nullptr;
    }
}

void QtLauncher::stopGame()
{
    if (!m_gameRunning || !m_gameProcess) return;

    m_gameProcess->terminate();
    if (!m_gameProcess->waitForFinished(3000)) {
        m_gameProcess->kill();
        m_gameProcess->waitForFinished(1000);
    }

    onGameFinished(m_gameProcess->exitCode(), m_gameProcess->exitStatus());
}

void QtLauncher::showSettings()
{
    // TODO: Implement settings dialog
    QMessageBox::information(this, "Settings", "Settings dialog not implemented yet.");
}

void QtLauncher::showAbout()
{
    QMessageBox::about(this, "About id Tech 3 Qt Launcher",
                      "<h3>id Tech 3 - Qt Edition</h3>"
                      "<p>A modern Qt-based launcher for id Tech 3 games.</p>"
                      "<p>Features:</p>"
                      "<ul>"
                      "<li>Modern Qt interface</li>"
                      "<li>Asset browser and editor</li>"
                      "<li>Performance profiler</li>"
                      "<li>Enhanced console</li>"
                      "<li>Mod management</li>"
                      "</ul>"
                      "<p>Version 1.0</p>");
}

void QtLauncher::checkForUpdates()
{
    // TODO: Implement update checking
    QMessageBox::information(this, "Updates", "Update checking not implemented yet.");
}

void QtLauncher::onGameLaunched()
{
    // Game launched successfully
    Com_Printf("Qt Launcher: Game launched successfully\n");
}

void QtLauncher::onGameFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    m_gameRunning = false;
    m_launchButton->setEnabled(true);
    m_stopButton->setEnabled(false);

    if (exitStatus == QProcess::NormalExit) {
        m_statusLabel->setText(QString("Game exited (code: %1)").arg(exitCode));
    } else {
        m_statusLabel->setText("Game crashed");
        QMessageBox::warning(this, "Game Error", "The game process terminated unexpectedly.");
    }

    delete m_gameProcess;
    m_gameProcess = nullptr;
}

void QtLauncher::onBrowseGamePath()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Select Game Executable",
                                                   m_gamePathEdit->text(),
                                                   "Executables (*.exe *.app *idtech3*);;All files (*)");
    if (!fileName.isEmpty()) {
        m_gamePathEdit->setText(fileName);

        // Auto-set working directory
        QFileInfo fileInfo(fileName);
        m_workingDirEdit->setText(fileInfo.absoluteDir().absolutePath());
    }
}

void QtLauncher::onBrowseWorkingDir()
{
    QString dirName = QFileDialog::getExistingDirectory(this, "Select Working Directory",
                                                       m_workingDirEdit->text());
    if (!dirName.isEmpty()) {
        m_workingDirEdit->setText(dirName);
    }
}

void QtLauncher::loadSettings()
{
    QSettings settings("id Software", "idTech3 Qt Launcher");

    m_gamePath = settings.value("gamePath", "").toString();
    m_workingDirectory = settings.value("workingDirectory", QDir::currentPath()).toString();
    m_modName = settings.value("modName", "baseq3").toString();
    m_mapName = settings.value("mapName", "q3dm1").toString();

    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void QtLauncher::saveSettings()
{
    QSettings settings("id Software", "idTech3 Qt Launcher");

    settings.setValue("gamePath", m_gamePathEdit->text());
    settings.setValue("workingDirectory", m_workingDirEdit->text());
    settings.setValue("modName", m_modEdit->text());
    settings.setValue("mapName", m_mapEdit->text());

    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void QtLauncher::updateUI()
{
    m_gamePathEdit->setText(m_gamePath);
    m_workingDirEdit->setText(m_workingDirectory);
    m_modEdit->setText(m_modName);
    m_mapEdit->setText(m_mapName);
}

void QtLauncher::closeEvent(QCloseEvent *event)
{
    if (m_gameRunning) {
        int result = QMessageBox::question(this, "Quit Launcher",
                                         "A game is currently running. Are you sure you want to quit?",
                                         QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::No) {
            event->ignore();
            return;
        }
        stopGame();
    }

    saveSettings();
    QMainWindow::closeEvent(event);
}

void QtLauncher::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    updateUI();
}

void QtLauncher::setGamePath(const QString& path)
{
    m_gamePath = path;
    m_gamePathEdit->setText(path);
}

//===============================================================================
// Main function
//===============================================================================

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Set application properties
    app.setApplicationName("idTech3 Qt Launcher");
    app.setApplicationDisplayName("id Tech 3 - Qt Launcher");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("id Software");

    // Create launcher window
    QtLauncher launcher;
    launcher.initialize();
    launcher.show();

    // Start Qt event loop
    return app.exec();
}

#include "qt_launcher.moc"