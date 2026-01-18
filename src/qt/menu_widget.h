/*
===============================================================================

Qt Main Menu Widget for id Tech 3

Provides a modern Qt-based main menu with game launching, settings, and mod management.

===============================================================================
*/

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QStackedWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QSlider>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QTextEdit>
#include <QLineEdit>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QScrollArea>
#include <QFrame>
#include <QTimer>
#include <QPropertyAnimation>
#include <QGraphicsEffect>
#include <QPixmap>
#include <QMovie>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <memory>
#include <vector>
#include <unordered_map>

// Forward declarations
class GameLauncher;
class SettingsWidget;
class ModManager;
class ServerBrowser;
class ProfileManager;

// Menu page types
enum class MenuPage {
    Main,
    SinglePlayer,
    Multiplayer,
    Settings,
    Mods,
    System,
    Credits,
    Loading
};

// Menu widget main class
class MenuWidget : public QWidget
{
    Q_OBJECT

public:
    MenuWidget(QWidget *parent = nullptr);
    ~MenuWidget() override;

    // Navigation
    void showPage(MenuPage page);
    MenuPage currentPage() const { return m_currentPage; }

    // Game launching
    void setGameLauncher(GameLauncher* launcher);
    GameLauncher* gameLauncher() const { return m_gameLauncher; }

    // Background
    void setBackgroundImage(const QString& imagePath);
    void setBackgroundVideo(const QString& videoPath);
    void setBackgroundColor(const QColor& color);

    // Animation
    void setPageTransitionEnabled(bool enabled);
    bool pageTransitionEnabled() const { return m_pageTransitionEnabled; }

signals:
    // Menu actions
    void actionTriggered(const QString& action);

    // Page changes
    void pageChanged(MenuPage oldPage, MenuPage newPage);

    // Game launching
    void gameLaunchRequested(const QString& mod, const QString& map);
    void gameJoinRequested(const QString& address);

public slots:
    // Navigation slots
    void onBackClicked();
    void onMainMenuClicked();

    // Action slots
    void onSinglePlayerClicked();
    void onMultiplayerClicked();
    void onSettingsClicked();
    void onModsClicked();
    void onSystemClicked();
    void onCreditsClicked();
    void onQuitClicked();

    // Game actions
    void onNewGameClicked();
    void onLoadGameClicked();
    void onJoinGameClicked();
    void onCreateServerClicked();

private slots:
    void onPageTransitionFinished();
    void onBackgroundDownloadFinished(QNetworkReply* reply);

private:
    void setupUI();
    void createMainPage();
    void createSinglePlayerPage();
    void createMultiplayerPage();
    void createSettingsPage();
    void createModsPage();
    void createSystemPage();
    void createCreditsPage();
    void createLoadingPage();

    void setupNavigation();
    void setupBackground();
    void setupAnimations();

    // Page transition animation
    void animatePageTransition(QWidget* fromPage, QWidget* toPage, bool forward = true);

    // Background management
    void updateBackground();
    void startBackgroundVideo();
    void stopBackgroundVideo();

    // UI components
    QVBoxLayout *m_mainLayout = nullptr;
    QStackedWidget *m_pageStack = nullptr;

    // Navigation
    QWidget *m_navigationWidget = nullptr;
    QHBoxLayout *m_navigationLayout = nullptr;
    QPushButton *m_backButton = nullptr;
    QLabel *m_titleLabel = nullptr;

    // Pages
    QWidget *m_mainPage = nullptr;
    QWidget *m_singlePlayerPage = nullptr;
    QWidget *m_multiplayerPage = nullptr;
    QWidget *m_settingsPage = nullptr;
    QWidget *m_modsPage = nullptr;
    QWidget *m_systemPage = nullptr;
    QWidget *m_creditsPage = nullptr;
    QWidget *m_loadingPage = nullptr;

    // Sub-widgets
    std::unique_ptr<GameLauncher> m_gameLauncher;
    std::unique_ptr<SettingsWidget> m_settingsWidget;
    std::unique_ptr<ModManager> m_modManager;
    std::unique_ptr<ServerBrowser> m_serverBrowser;
    std::unique_ptr<ProfileManager> m_profileManager;

    // Background
    QLabel *m_backgroundLabel = nullptr;
    QMovie *m_backgroundMovie = nullptr;
    QWidget *m_backgroundOverlay = nullptr;

    // Animation
    QPropertyAnimation *m_pageAnimation = nullptr;
    bool m_pageTransitionEnabled = true;

    // State
    MenuPage m_currentPage = MenuPage::Main;
    QColor m_backgroundColor = QColor(32, 32, 32);
    QString m_backgroundImagePath;
    QString m_backgroundVideoPath;

    // Network for background downloads
    QNetworkAccessManager *m_networkManager = nullptr;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
};

//===============================================================================
// Game Launcher Widget
//===============================================================================

class GameLauncher : public QWidget
{
    Q_OBJECT

public:
    GameLauncher(QWidget *parent = nullptr);
    ~GameLauncher() override;

    // Game configuration
    void setModList(const QStringList& mods);
    void setMapList(const QStringList& maps);
    void setDifficultyList(const QStringList& difficulties);

    QString selectedMod() const;
    QString selectedMap() const;
    QString selectedDifficulty() const;

    // Launch options
    bool dedicatedServer() const;
    bool lanOnly() const;
    int maxClients() const;
    QString serverName() const;
    QString password() const;

signals:
    void launchRequested(const QString& mod, const QString& map, const QString& difficulty);
    void serverLaunchRequested(const QString& mod, const QString& map);

private slots:
    void onLaunchClicked();
    void onServerLaunchClicked();
    void onModChanged(const QString& mod);
    void updateMapList();

private:
    void setupUI();

    QVBoxLayout *m_layout = nullptr;

    // Game selection
    QGroupBox *m_gameGroup = nullptr;
    QComboBox *m_modCombo = nullptr;
    QComboBox *m_mapCombo = nullptr;
    QComboBox *m_difficultyCombo = nullptr;

    // Server options
    QGroupBox *m_serverGroup = nullptr;
    QCheckBox *m_dedicatedCheck = nullptr;
    QCheckBox *m_lanOnlyCheck = nullptr;
    QSpinBox *m_maxClientsSpin = nullptr;
    QLineEdit *m_serverNameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;

    // Launch buttons
    QHBoxLayout *m_buttonLayout = nullptr;
    QPushButton *m_launchButton = nullptr;
    QPushButton *m_serverButton = nullptr;
    QPushButton *m_cancelButton = nullptr;

    // Data
    QStringList m_modList;
    std::unordered_map<QString, QStringList> m_modMaps;
};

//===============================================================================
// Settings Widget
//===============================================================================

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    SettingsWidget(QWidget *parent = nullptr);
    ~SettingsWidget() override;

    // Load/save settings
    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();

private slots:
    void onVideoSettingsChanged();
    void onAudioSettingsChanged();
    void onInputSettingsChanged();
    void onGameplaySettingsChanged();
    void onNetworkSettingsChanged();

    void onApplyClicked();
    void onResetClicked();
    void onDefaultsClicked();

private:
    void setupUI();
    void createVideoTab();
    void createAudioTab();
    void createInputTab();
    void createGameplayTab();
    void createNetworkTab();

    QTabWidget *m_tabWidget = nullptr;

    // Video settings
    QComboBox *m_resolutionCombo = nullptr;
    QComboBox *m_displayModeCombo = nullptr;
    QComboBox *m_vsyncCombo = nullptr;
    QSlider *m_brightnessSlider = nullptr;
    QSlider *m_gammaSlider = nullptr;
    QCheckBox *m_fullscreenCheck = nullptr;
    QCheckBox *m_hdrCheck = nullptr;

    // Audio settings
    QSlider *m_masterVolumeSlider = nullptr;
    QSlider *m_musicVolumeSlider = nullptr;
    QSlider *m_sfxVolumeSlider = nullptr;
    QComboBox *m_audioDeviceCombo = nullptr;
    QCheckBox *m_subtitlesCheck = nullptr;

    // Input settings
    QComboBox *m_mouseSensitivityCombo = nullptr;
    QCheckBox *m_invertMouseCheck = nullptr;
    QCheckBox *m_rawMouseCheck = nullptr;
    QTreeWidget *m_keyBindingsTree = nullptr;

    // Gameplay settings
    QComboBox *m_difficultyCombo = nullptr;
    QCheckBox *m_autoSaveCheck = nullptr;
    QCheckBox *m_quickSaveCheck = nullptr;
    QSlider *m_mouseLookSpeedSlider = nullptr;

    // Network settings
    QLineEdit *m_playerNameEdit = nullptr;
    QComboBox *m_maxFPSCombo = nullptr;
    QComboBox *m_rateCombo = nullptr;
    QCheckBox *m_allowDownloadCheck = nullptr;

    // Buttons
    QHBoxLayout *m_buttonLayout = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_resetButton = nullptr;
    QPushButton *m_defaultsButton = nullptr;
};

//===============================================================================
// Mod Manager Widget
//===============================================================================

class ModManager : public QWidget
{
    Q_OBJECT

public:
    ModManager(QWidget *parent = nullptr);
    ~ModManager() override;

    // Mod operations
    void refreshModList();
    bool installMod(const QString& modPath);
    bool uninstallMod(const QString& modName);
    bool enableMod(const QString& modName);
    bool disableMod(const QString& modName);

    QStringList enabledMods() const;

signals:
    void modListChanged();
    void modStateChanged(const QString& modName, bool enabled);

private slots:
    void onModItemChanged(QListWidgetItem* item);
    void onInstallClicked();
    void onUninstallClicked();
    void onEnableClicked();
    void onDisableClicked();
    void onModInfoClicked();

private:
    void setupUI();
    void populateModList();
    void updateModInfo(const QString& modName);

    QVBoxLayout *m_layout = nullptr;

    // Mod list
    QListWidget *m_modList = nullptr;

    // Mod info
    QGroupBox *m_infoGroup = nullptr;
    QLabel *m_modNameLabel = nullptr;
    QLabel *m_modVersionLabel = nullptr;
    QLabel *m_modAuthorLabel = nullptr;
    QLabel *m_modDescriptionLabel = nullptr;
    QTextEdit *m_modReadmeText = nullptr;

    // Buttons
    QHBoxLayout *m_buttonLayout = nullptr;
    QPushButton *m_installButton = nullptr;
    QPushButton *m_uninstallButton = nullptr;
    QPushButton *m_enableButton = nullptr;
    QPushButton *m_disableButton = nullptr;
    QPushButton *m_infoButton = nullptr;

    // Data
    struct ModInfo {
        QString name;
        QString version;
        QString author;
        QString description;
        QString readme;
        bool enabled;
        bool installed;
    };

    std::vector<ModInfo> m_mods;
};

//===============================================================================
// Server Browser Widget
//===============================================================================

class ServerBrowser : public QWidget
{
    Q_OBJECT

public:
    ServerBrowser(QWidget *parent = nullptr);
    ~ServerBrowser() override;

    // Server operations
    void refreshServerList();
    void connectToServer(const QString& address);
    void addFavoriteServer(const QString& address);
    void removeFavoriteServer(const QString& address);

signals:
    void serverConnected(const QString& address);
    void serverDoubleClicked(const QString& address);

private slots:
    void onRefreshClicked();
    void onConnectClicked();
    void onAddFavoriteClicked();
    void onRemoveFavoriteClicked();
    void onServerDoubleClicked(QTableWidgetItem* item);
    void onFilterChanged();
    void onServerListContextMenu(const QPoint& pos);

private:
    void setupUI();
    void setupServerTable();
    void populateServerList();
    void updateServerInfo();
    void createContextMenu();

    QVBoxLayout *m_layout = nullptr;

    // Toolbar
    QHBoxLayout *m_toolbarLayout = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLineEdit *m_filterEdit = nullptr;
    QComboBox *m_gameTypeCombo = nullptr;
    QComboBox *m_regionCombo = nullptr;

    // Server list
    QTableWidget *m_serverTable = nullptr;

    // Server info
    QGroupBox *m_infoGroup = nullptr;
    QLabel *m_serverNameLabel = nullptr;
    QLabel *m_mapNameLabel = nullptr;
    QLabel *m_playerCountLabel = nullptr;
    QLabel *m_pingLabel = nullptr;
    QLabel *m_gameTypeLabel = nullptr;
    QTextEdit *m_serverRulesText = nullptr;

    // Buttons
    QHBoxLayout *m_buttonLayout = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_addFavoriteButton = nullptr;
    QPushButton *m_removeFavoriteButton = nullptr;

    // Context menu
    QMenu *m_contextMenu = nullptr;

    // Data
    struct ServerInfo {
        QString name;
        QString address;
        QString map;
        int players;
        int maxPlayers;
        int ping;
        QString gameType;
        QString version;
        bool password;
        bool dedicated;
        QStringList rules;
    };

    std::vector<ServerInfo> m_servers;
    std::vector<QString> m_favorites;
};