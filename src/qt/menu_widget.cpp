/*
===============================================================================

Qt Main Menu Widget Implementation

===============================================================================
*/

#include "menu_widget.h"
#include "../common/qcommon.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QPixmap>
#include <QMovie>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QGraphicsOpacityEffect>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QProgressDialog>
#include <QSystemTrayIcon>
#include <QTimer>

//===============================================================================
// MenuWidget
//===============================================================================

MenuWidget::MenuWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupNavigation();
    setupBackground();
    setupAnimations();

    // Create network manager for background downloads
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &MenuWidget::onBackgroundDownloadFinished);

    showPage(MenuPage::Main);
}

MenuWidget::~MenuWidget()
{
}

void MenuWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Background widget
    m_backgroundLabel = new QLabel(this);
    m_backgroundLabel->setScaledContents(true);
    m_backgroundLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_mainLayout->addWidget(m_backgroundLabel);

    // Background overlay for better text readability
    m_backgroundOverlay = new QWidget(m_backgroundLabel);
    m_backgroundOverlay->setStyleSheet("background-color: rgba(0, 0, 0, 128);");
    m_backgroundOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);

    // Navigation widget
    m_navigationWidget = new QWidget(m_backgroundOverlay);
    m_navigationWidget->setFixedHeight(50);
    m_navigationWidget->setStyleSheet(
        "QWidget { background-color: rgba(32, 32, 32, 192); border-bottom: 1px solid rgba(255, 255, 255, 64); }"
    );

    m_navigationLayout = new QHBoxLayout(m_navigationWidget);
    m_navigationLayout->setContentsMargins(20, 0, 20, 0);

    m_backButton = new QPushButton("← Back", m_navigationWidget);
    m_backButton->setFixedSize(80, 30);
    m_backButton->setStyleSheet(
        "QPushButton { background-color: rgba(64, 64, 64, 192); color: white; border: 1px solid rgba(255, 255, 255, 64); border-radius: 4px; }"
        "QPushButton:hover { background-color: rgba(96, 96, 96, 192); }"
        "QPushButton:pressed { background-color: rgba(128, 128, 128, 192); }"
    );
    connect(m_backButton, &QPushButton::clicked, this, &MenuWidget::onBackClicked);

    m_titleLabel = new QLabel("id Tech 3", m_navigationWidget);
    m_titleLabel->setStyleSheet("color: white; font-size: 18px; font-weight: bold;");
    m_titleLabel->setAlignment(Qt::AlignCenter);

    m_navigationLayout->addWidget(m_backButton);
    m_navigationLayout->addStretch();
    m_navigationLayout->addWidget(m_titleLabel);
    m_navigationLayout->addStretch();

    // Page stack
    m_pageStack = new QStackedWidget(m_backgroundOverlay);
    m_pageStack->setStyleSheet("QStackedWidget { background: transparent; }");

    // Create pages
    createMainPage();
    createSinglePlayerPage();
    createMultiplayerPage();
    createSettingsPage();
    createModsPage();
    createSystemPage();
    createCreditsPage();
    createLoadingPage();

    // Layout background overlay
    QVBoxLayout *overlayLayout = new QVBoxLayout(m_backgroundOverlay);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(m_navigationWidget);
    overlayLayout->addWidget(m_pageStack, 1);

    m_backgroundOverlay->setLayout(overlayLayout);
}

void MenuWidget::createMainPage()
{
    m_mainPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_mainPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    // Logo/title
    QLabel *logoLabel = new QLabel("id Tech 3", m_mainPage);
    logoLabel->setStyleSheet("color: white; font-size: 48px; font-weight: bold; margin-bottom: 40px;");
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);

    // Main menu buttons
    QString buttonStyle =
        "QPushButton { "
        "    background-color: rgba(64, 64, 64, 192); "
        "    color: white; "
        "    border: 1px solid rgba(255, 255, 255, 64); "
        "    border-radius: 8px; "
        "    padding: 12px 24px; "
        "    font-size: 16px; "
        "    min-width: 200px; "
        "    min-height: 50px; "
        "} "
        "QPushButton:hover { "
        "    background-color: rgba(96, 96, 96, 192); "
        "    border-color: rgba(255, 255, 255, 128); "
        "} "
        "QPushButton:pressed { "
        "    background-color: rgba(128, 128, 128, 192); "
        "}";

    QPushButton *singlePlayerBtn = new QPushButton("Single Player", m_mainPage);
    singlePlayerBtn->setStyleSheet(buttonStyle);
    connect(singlePlayerBtn, &QPushButton::clicked, this, &MenuWidget::onSinglePlayerClicked);
    layout->addWidget(singlePlayerBtn);

    QPushButton *multiplayerBtn = new QPushButton("Multiplayer", m_mainPage);
    multiplayerBtn->setStyleSheet(buttonStyle);
    connect(multiplayerBtn, &QPushButton::clicked, this, &MenuWidget::onMultiplayerClicked);
    layout->addWidget(multiplayerBtn);

    QPushButton *settingsBtn = new QPushButton("Settings", m_mainPage);
    settingsBtn->setStyleSheet(buttonStyle);
    connect(settingsBtn, &QPushButton::clicked, this, &MenuWidget::onSettingsClicked);
    layout->addWidget(settingsBtn);

    QPushButton *modsBtn = new QPushButton("Mods", m_mainPage);
    modsBtn->setStyleSheet(buttonStyle);
    connect(modsBtn, &QPushButton::clicked, this, &MenuWidget::onModsClicked);
    layout->addWidget(modsBtn);

    QPushButton *systemBtn = new QPushButton("System", m_mainPage);
    systemBtn->setStyleSheet(buttonStyle);
    connect(systemBtn, &QPushButton::clicked, this, &MenuWidget::onSystemClicked);
    layout->addWidget(systemBtn);

    layout->addStretch();

    // Bottom buttons
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(20);

    QPushButton *creditsBtn = new QPushButton("Credits", m_mainPage);
    creditsBtn->setStyleSheet("QPushButton { background: transparent; color: rgba(255, 255, 255, 128); border: none; } QPushButton:hover { color: white; }");
    connect(creditsBtn, &QPushButton::clicked, this, &MenuWidget::onCreditsClicked);

    QPushButton *quitBtn = new QPushButton("Quit", m_mainPage);
    quitBtn->setStyleSheet("QPushButton { background: rgba(128, 32, 32, 192); color: white; border: 1px solid rgba(255, 64, 64, 128); border-radius: 4px; padding: 8px 16px; } QPushButton:hover { background-color: rgba(160, 48, 48, 192); }");
    connect(quitBtn, &QPushButton::clicked, this, &MenuWidget::onQuitClicked);

    bottomLayout->addStretch();
    bottomLayout->addWidget(creditsBtn);
    bottomLayout->addWidget(quitBtn);

    layout->addLayout(bottomLayout);
    m_pageStack->addWidget(m_mainPage);
}

void MenuWidget::createSinglePlayerPage()
{
    m_singlePlayerPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_singlePlayerPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("Single Player", m_singlePlayerPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // Game launcher widget
    m_gameLauncher = std::make_unique<GameLauncher>(m_singlePlayerPage);
    connect(m_gameLauncher.get(), &GameLauncher::launchRequested,
            this, [this](const QString& mod, const QString& map, const QString& difficulty) {
                emit gameLaunchRequested(mod, map);
            });
    layout->addWidget(m_gameLauncher.get());

    m_pageStack->addWidget(m_singlePlayerPage);
}

void MenuWidget::createMultiplayerPage()
{
    m_multiplayerPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_multiplayerPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("Multiplayer", m_multiplayerPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // TODO: Add server browser, create server, join game options
    QLabel *placeholder = new QLabel("Multiplayer features coming soon...", m_multiplayerPage);
    placeholder->setStyleSheet("color: rgba(255, 255, 255, 128); font-size: 18px;");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    m_pageStack->addWidget(m_multiplayerPage);
}

void MenuWidget::createSettingsPage()
{
    m_settingsPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_settingsPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("Settings", m_settingsPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // TODO: Add settings tabs (Video, Audio, Input, Gameplay, Network)
    QLabel *placeholder = new QLabel("Settings panel coming soon...", m_settingsPage);
    placeholder->setStyleSheet("color: rgba(255, 255, 255, 128); font-size: 18px;");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    m_pageStack->addWidget(m_settingsPage);
}

void MenuWidget::createModsPage()
{
    m_modsPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_modsPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("Mod Manager", m_modsPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // TODO: Add mod list, install/uninstall/enable/disable functionality
    QLabel *placeholder = new QLabel("Mod management features coming soon...", m_modsPage);
    placeholder->setStyleSheet("color: rgba(255, 255, 255, 128); font-size: 18px;");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    m_pageStack->addWidget(m_modsPage);
}

void MenuWidget::createSystemPage()
{
    m_systemPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_systemPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("System", m_systemPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    // TODO: Add system info, console, profiler access
    QLabel *placeholder = new QLabel("System tools coming soon...", m_systemPage);
    placeholder->setStyleSheet("color: rgba(255, 255, 255, 128); font-size: 18px;");
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    m_pageStack->addWidget(m_systemPage);
}

void MenuWidget::createCreditsPage()
{
    m_creditsPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_creditsPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *titleLabel = new QLabel("Credits", m_creditsPage);
    titleLabel->setStyleSheet("color: white; font-size: 32px; font-weight: bold; margin-bottom: 20px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    QTextEdit *creditsText = new QTextEdit(m_creditsPage);
    creditsText->setReadOnly(true);
    creditsText->setStyleSheet(
        "QTextEdit { background-color: rgba(32, 32, 32, 192); color: white; border: 1px solid rgba(255, 255, 255, 64); border-radius: 8px; }"
    );
    creditsText->setPlainText(
        "id Tech 3 - Enhanced Edition\n\n"
        "Original id Tech 3 Engine\n"
        "Copyright (C) 1999-2005 Id Software, Inc.\n\n"
        "Qt Integration\n"
        "Modern UI and enhanced features\n\n"
        "Contributors:\n"
        "- id Software\n"
        "- Open Source Community\n"
        "- Qt Project\n\n"
        "Special Thanks:\n"
        "- John Carmack\n"
        "- The id Tech Community\n"
        "- All the modders and players"
    );
    layout->addWidget(creditsText);

    m_pageStack->addWidget(m_creditsPage);
}

void MenuWidget::createLoadingPage()
{
    m_loadingPage = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(m_loadingPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);

    QLabel *loadingLabel = new QLabel("Loading...", m_loadingPage);
    loadingLabel->setStyleSheet("color: white; font-size: 24px; font-weight: bold;");
    loadingLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(loadingLabel);

    QProgressBar *progressBar = new QProgressBar(m_loadingPage);
    progressBar->setRange(0, 100);
    progressBar->setValue(50);
    progressBar->setStyleSheet(
        "QProgressBar { background-color: rgba(32, 32, 32, 192); border: 1px solid rgba(255, 255, 255, 64); border-radius: 4px; text-align: center; color: white; }"
        "QProgressBar::chunk { background-color: rgba(64, 128, 255, 192); }"
    );
    layout->addWidget(progressBar);

    m_pageStack->addWidget(m_loadingPage);
}

void MenuWidget::setupNavigation()
{
    // Navigation is already set up in setupUI()
}

void MenuWidget::setupBackground()
{
    // Set default background color
    updateBackground();

    // TODO: Load background image/video from game assets
    setBackgroundImage(":/backgrounds/main_menu.jpg");
}

void MenuWidget::setupAnimations()
{
    m_pageAnimation = new QPropertyAnimation(this);
    m_pageAnimation->setDuration(300);
    m_pageAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_pageAnimation, &QPropertyAnimation::finished,
            this, &MenuWidget::onPageTransitionFinished);
}

void MenuWidget::showPage(MenuPage page)
{
    if (page == m_currentPage) return;

    MenuPage oldPage = m_currentPage;
    m_currentPage = page;

    // Update navigation
    m_backButton->setVisible(page != MenuPage::Main);
    m_titleLabel->setText(page == MenuPage::Main ? "id Tech 3" :
                         page == MenuPage::SinglePlayer ? "Single Player" :
                         page == MenuPage::Multiplayer ? "Multiplayer" :
                         page == MenuPage::Settings ? "Settings" :
                         page == MenuPage::Mods ? "Mods" :
                         page == MenuPage::System ? "System" :
                         page == MenuPage::Credits ? "Credits" : "Loading");

    // Show page with animation if enabled
    if (m_pageTransitionEnabled) {
        QWidget *currentWidget = m_pageStack->currentWidget();
        QWidget *nextWidget = nullptr;

        switch (page) {
            case MenuPage::Main: nextWidget = m_mainPage; break;
            case MenuPage::SinglePlayer: nextWidget = m_singlePlayerPage; break;
            case MenuPage::Multiplayer: nextWidget = m_multiplayerPage; break;
            case MenuPage::Settings: nextWidget = m_settingsPage; break;
            case MenuPage::Mods: nextWidget = m_modsPage; break;
            case MenuPage::System: nextWidget = m_systemPage; break;
            case MenuPage::Credits: nextWidget = m_creditsPage; break;
            case MenuPage::Loading: nextWidget = m_loadingPage; break;
        }

        if (currentWidget && nextWidget) {
            animatePageTransition(currentWidget, nextWidget);
        } else if (nextWidget) {
            m_pageStack->setCurrentWidget(nextWidget);
        }
    } else {
        switch (page) {
            case MenuPage::Main: m_pageStack->setCurrentWidget(m_mainPage); break;
            case MenuPage::SinglePlayer: m_pageStack->setCurrentWidget(m_singlePlayerPage); break;
            case MenuPage::Multiplayer: m_pageStack->setCurrentWidget(m_multiplayerPage); break;
            case MenuPage::Settings: m_pageStack->setCurrentWidget(m_settingsPage); break;
            case MenuPage::Mods: m_pageStack->setCurrentWidget(m_modsPage); break;
            case MenuPage::System: m_pageStack->setCurrentWidget(m_systemPage); break;
            case MenuPage::Credits: m_pageStack->setCurrentWidget(m_creditsPage); break;
            case MenuPage::Loading: m_pageStack->setCurrentWidget(m_loadingPage); break;
        }
    }

    emit pageChanged(oldPage, page);
}

void MenuWidget::animatePageTransition(QWidget* fromPage, QWidget* toPage, bool forward)
{
    // Simple fade transition
    QGraphicsOpacityEffect *fromEffect = new QGraphicsOpacityEffect(fromPage);
    QGraphicsOpacityEffect *toEffect = new QGraphicsOpacityEffect(toPage);

    fromPage->setGraphicsEffect(fromEffect);
    toPage->setGraphicsEffect(toEffect);

    m_pageStack->setCurrentWidget(toPage);
    toEffect->setOpacity(0.0);

    QPropertyAnimation *fadeOut = new QPropertyAnimation(fromEffect, "opacity");
    fadeOut->setDuration(150);
    fadeOut->setStartValue(1.0);
    fadeOut->setEndValue(0.0);

    QPropertyAnimation *fadeIn = new QPropertyAnimation(toEffect, "opacity");
    fadeIn->setDuration(150);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);

    QParallelAnimationGroup *group = new QParallelAnimationGroup;
    group->addAnimation(fadeOut);
    group->addAnimation(fadeIn);

    connect(group, &QParallelAnimationGroup::finished, [fromEffect, toEffect, fromPage, toPage]() {
        fromPage->setGraphicsEffect(nullptr);
        toPage->setGraphicsEffect(nullptr);
        delete fromEffect;
        delete toEffect;
    });

    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MenuWidget::setBackgroundImage(const QString& imagePath)
{
    m_backgroundImagePath = imagePath;
    updateBackground();
}

void MenuWidget::setBackgroundVideo(const QString& videoPath)
{
    m_backgroundVideoPath = videoPath;
    updateBackground();
}

void MenuWidget::setBackgroundColor(const QColor& color)
{
    m_backgroundColor = color;
    updateBackground();
}

void MenuWidget::updateBackground()
{
    if (!m_backgroundLabel) return;

    if (!m_backgroundImagePath.isEmpty()) {
        QPixmap pixmap(m_backgroundImagePath);
        if (!pixmap.isNull()) {
            m_backgroundLabel->setPixmap(pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        } else {
            // Fallback to color
            m_backgroundLabel->setStyleSheet(QString("background-color: %1;").arg(m_backgroundColor.name()));
        }
    } else if (!m_backgroundVideoPath.isEmpty()) {
        // TODO: Implement video background
        m_backgroundLabel->setStyleSheet(QString("background-color: %1;").arg(m_backgroundColor.name()));
    } else {
        m_backgroundLabel->setStyleSheet(QString("background-color: %1;").arg(m_backgroundColor.name()));
    }
}

void MenuWidget::setGameLauncher(GameLauncher* launcher)
{
    m_gameLauncher.reset(launcher);
}

void MenuWidget::onBackClicked()
{
    showPage(MenuPage::Main);
}

void MenuWidget::onMainMenuClicked()
{
    showPage(MenuPage::Main);
}

void MenuWidget::onSinglePlayerClicked()
{
    showPage(MenuPage::SinglePlayer);
}

void MenuWidget::onMultiplayerClicked()
{
    showPage(MenuPage::Multiplayer);
}

void MenuWidget::onSettingsClicked()
{
    showPage(MenuPage::Settings);
}

void MenuWidget::onModsClicked()
{
    showPage(MenuPage::Mods);
}

void MenuWidget::onSystemClicked()
{
    showPage(MenuPage::System);
}

void MenuWidget::onCreditsClicked()
{
    showPage(MenuPage::Credits);
}

void MenuWidget::onQuitClicked()
{
    emit actionTriggered("quit");
}

void MenuWidget::onPageTransitionFinished()
{
    // Cleanup after animation
}

void MenuWidget::onBackgroundDownloadFinished(QNetworkReply* reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QPixmap pixmap;
        pixmap.loadFromData(data);
        if (!pixmap.isNull()) {
            m_backgroundLabel->setPixmap(pixmap.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
        }
    }
    reply->deleteLater();
}

void MenuWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    // Resize background overlay to match parent
    if (m_backgroundOverlay) {
        m_backgroundOverlay->setGeometry(rect());
    }

    // Update background scaling
    updateBackground();
}

void MenuWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    updateBackground();
}

void MenuWidget::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    stopBackgroundVideo();
}

void MenuWidget::startBackgroundVideo()
{
    if (m_backgroundMovie && !m_backgroundVideoPath.isEmpty()) {
        m_backgroundMovie->start();
    }
}

void MenuWidget::stopBackgroundVideo()
{
    if (m_backgroundMovie) {
        m_backgroundMovie->stop();
    }
}

//===============================================================================
// GameLauncher Implementation
//===============================================================================

GameLauncher::GameLauncher(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

GameLauncher::~GameLauncher()
{
}

void GameLauncher::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(10);

    // Game selection group
    m_gameGroup = new QGroupBox("Game Settings", this);
    QFormLayout *gameLayout = new QFormLayout(m_gameGroup);

    m_modCombo = new QComboBox(m_gameGroup);
    m_modCombo->addItem("baseq3", "Base Game");
    m_modCombo->addItem("missionpack", "Team Arena");
    gameLayout->addRow("Mod:", m_modCombo);

    m_mapCombo = new QComboBox(m_gameGroup);
    m_mapCombo->addItem("q3dm1", "Arena of Death");
    m_mapCombo->addItem("q3dm7", "Temple of Retribution");
    gameLayout->addRow("Map:", m_mapCombo);

    m_difficultyCombo = new QComboBox(m_gameGroup);
    m_difficultyCombo->addItem("easy", "Recruit");
    m_difficultyCombo->addItem("medium", "Soldier");
    m_difficultyCombo->addItem("hard", "Elite");
    gameLayout->addRow("Difficulty:", m_difficultyCombo);

    m_layout->addWidget(m_gameGroup);

    // Launch buttons
    m_buttonLayout = new QHBoxLayout();

    m_launchButton = new QPushButton("Launch Game", this);
    m_launchButton->setStyleSheet(
        "QPushButton { background-color: rgba(64, 128, 64, 192); color: white; border: 1px solid rgba(128, 255, 128, 128); border-radius: 4px; padding: 8px 16px; }"
        "QPushButton:hover { background-color: rgba(96, 160, 96, 192); }"
    );
    connect(m_launchButton, &QPushButton::clicked, this, &GameLauncher::onLaunchClicked);

    m_cancelButton = new QPushButton("Cancel", this);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() { emit launchRequested("", "", ""); });

    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_cancelButton);
    m_buttonLayout->addWidget(m_launchButton);

    m_layout->addLayout(m_buttonLayout);

    connect(m_modCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &GameLauncher::onModChanged);
}

void GameLauncher::setModList(const QStringList& mods)
{
    m_modCombo->clear();
    m_modCombo->addItems(mods);
}

void GameLauncher::setMapList(const QStringList& maps)
{
    m_mapCombo->clear();
    m_mapCombo->addItems(maps);
}

void GameLauncher::setDifficultyList(const QStringList& difficulties)
{
    m_difficultyCombo->clear();
    m_difficultyCombo->addItems(difficulties);
}

QString GameLauncher::selectedMod() const
{
    return m_modCombo->currentText();
}

QString GameLauncher::selectedMap() const
{
    return m_mapCombo->currentText();
}

QString GameLauncher::selectedDifficulty() const
{
    return m_difficultyCombo->currentText();
}

void GameLauncher::onLaunchClicked()
{
    emit launchRequested(selectedMod(), selectedMap(), selectedDifficulty());
}

void GameLauncher::onModChanged(const QString& mod)
{
    // Update map list based on selected mod
    updateMapList();
}

void GameLauncher::updateMapList()
{
    QString mod = selectedMod();

    m_mapCombo->clear();

    if (mod == "baseq3") {
        m_mapCombo->addItem("q3dm1", "Arena of Death");
        m_mapCombo->addItem("q3dm7", "Temple of Retribution");
        m_mapCombo->addItem("q3dm17", "The Longest Yard");
    } else if (mod == "missionpack") {
        m_mapCombo->addItem("mpq3dm1", "Arena of Death (MP)");
        m_mapCombo->addItem("mpq3dm2", "Space Chamber");
    }

    // TODO: Load actual map list from filesystem
}