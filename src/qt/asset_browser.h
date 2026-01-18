/*
===============================================================================

Qt Asset Browser for id Tech 3

Provides a modern file browser interface for game assets with preview and editing capabilities.

===============================================================================
*/

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QProgressBar>
#include <QTabWidget>
#include <QMenu>
#include <QAction>
#include <QFileSystemModel>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

// Forward declarations
class AssetPreviewWidget;
class AssetPropertyEditor;
class AssetSearchWidget;

// Asset types
enum class AssetType {
    Unknown = 0,
    Texture,
    Model,
    Sound,
    Shader,
    Material,
    Animation,
    Level,
    Script,
    Config,
    PakFile
};

// Asset information structure
struct AssetInfo {
    QString name;
    QString path;
    QString fullPath;
    AssetType type;
    qint64 size;
    QDateTime modified;
    QString description;
    std::unordered_map<std::string, std::string> metadata;

    AssetInfo() : type(AssetType::Unknown), size(0) {}
};

// Asset browser main class
class AssetBrowser : public QWidget
{
    Q_OBJECT

public:
    AssetBrowser(QWidget *parent = nullptr);
    ~AssetBrowser() override;

    // Asset management
    void setBasePath(const QString& path);
    QString basePath() const { return m_basePath; }

    void refreshAssets();
    void scanDirectory(const QString& path);

    // Selection
    AssetInfo selectedAsset() const;
    QList<AssetInfo> selectedAssets() const;

    // Filtering
    void setFilterType(AssetType type);
    void setFilterText(const QString& text);
    AssetType filterType() const { return m_filterType; }

    // Preview
    void enablePreview(bool enabled);
    bool previewEnabled() const { return m_previewEnabled; }

signals:
    // Asset events
    void assetSelected(const AssetInfo& asset);
    void assetDoubleClicked(const AssetInfo& asset);
    void assetContextMenuRequested(const AssetInfo& asset, const QPoint& pos);

    // Browser events
    void directoryChanged(const QString& path);
    void assetsRefreshed();

public slots:
    // Navigation
    void navigateToPath(const QString& path);
    void goUp();
    void goBack();
    void goForward();

    // Asset operations
    void importAsset();
    void exportAsset();
    void deleteAsset();
    void renameAsset();
    void duplicateAsset();

    // View modes
    void setViewMode(int mode); // 0=list, 1=icons, 2=details
    void sortByName();
    void sortByType();
    void sortBySize();
    void sortByDate();

private slots:
    void onAssetItemClicked(QListWidgetItem* item);
    void onAssetItemDoubleClicked(QListWidgetItem* item);
    void onAssetContextMenu(const QPoint& pos);
    void onDirectoryChanged(const QString& path);
    void onSearchTextChanged(const QString& text);
    void onFilterTypeChanged(int index);
    void onRefreshClicked();

private:
    void setupUI();
    void createToolbar();
    void createTreeView();
    void createAssetView();
    void createPreviewPanel();
    void createPropertyPanel();
    void setupConnections();

    // Asset scanning
    void scanDirectoryRecursive(const QString& path, const QString& basePath);
    AssetType detectAssetType(const QString& filename);
    AssetInfo createAssetInfo(const QFileInfo& fileInfo, const QString& basePath);

    // UI updates
    void updateAssetList();
    void updatePreview(const AssetInfo& asset);
    void updateProperties(const AssetInfo& asset);
    void updateNavigationButtons();

    // Utility
    QString formatFileSize(qint64 size) const;
    QIcon getAssetIcon(AssetType type) const;
    QString getAssetTypeName(AssetType type) const;

    // UI components
    QVBoxLayout *m_mainLayout = nullptr;
    QSplitter *m_mainSplitter = nullptr;

    // Left panel - directory tree
    QGroupBox *m_treeGroup = nullptr;
    QTreeWidget *m_directoryTree = nullptr;
    QFileSystemModel *m_fsModel = nullptr;

    // Right panel - assets and preview
    QSplitter *m_rightSplitter = nullptr;

    // Asset list/view
    QGroupBox *m_assetGroup = nullptr;
    QVBoxLayout *m_assetLayout = nullptr;

    // Toolbar
    QHBoxLayout *m_toolbarLayout = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_forwardButton = nullptr;
    QPushButton *m_upButton = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_pathLabel = nullptr;

    // Search and filter
    QHBoxLayout *m_filterLayout = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_typeFilterCombo = nullptr;
    QComboBox *m_viewModeCombo = nullptr;

    // Asset display
    QListWidget *m_assetList = nullptr;
    QTableWidget *m_assetTable = nullptr;
    QSortFilterProxyModel *m_assetProxyModel = nullptr;

    // Preview panel
    QGroupBox *m_previewGroup = nullptr;
    AssetPreviewWidget *m_previewWidget = nullptr;

    // Properties panel
    QGroupBox *m_propertyGroup = nullptr;
    AssetPropertyEditor *m_propertyEditor = nullptr;

    // Context menu
    QMenu *m_contextMenu = nullptr;

    // Data
    QString m_basePath;
    QString m_currentPath;
    std::vector<QString> m_navigationHistory;
    int m_historyIndex = -1;

    std::vector<AssetInfo> m_allAssets;
    std::vector<AssetInfo> m_filteredAssets;

    AssetType m_filterType = AssetType::Unknown;
    QString m_searchText;
    int m_viewMode = 0; // 0=list, 1=icons, 2=table
    bool m_previewEnabled = true;

    // Scanning state
    bool m_isScanning = false;
    QTimer m_scanTimer;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

//===============================================================================
// Asset Preview Widget
//===============================================================================

class AssetPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    AssetPreviewWidget(QWidget *parent = nullptr);
    ~AssetPreviewWidget() override;

    void setAsset(const AssetInfo& asset);
    void clearPreview();

private:
    void setupUI();
    void updateTexturePreview(const AssetInfo& asset);
    void updateModelPreview(const AssetInfo& asset);
    void updateSoundPreview(const AssetInfo& asset);
    void updateTextPreview(const AssetInfo& asset);

    // UI components
    QVBoxLayout *m_layout = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
    QTextEdit *m_infoText = nullptr;

    // Preview data
    AssetInfo m_currentAsset;
};

//===============================================================================
// Asset Property Editor
//===============================================================================

class AssetPropertyEditor : public QWidget
{
    Q_OBJECT

public:
    AssetPropertyEditor(QWidget *parent = nullptr);
    ~AssetPropertyEditor() override;

    void setAsset(const AssetInfo& asset);
    void clearProperties();

signals:
    void propertyChanged(const QString& property, const QVariant& value);

private slots:
    void onPropertyValueChanged();

private:
    void setupUI();
    void createBasicProperties(const AssetInfo& asset);
    void createTypeSpecificProperties(const AssetInfo& asset);

    // UI components
    QVBoxLayout *m_layout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_contentWidget = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;

    // Property widgets
    std::unordered_map<std::string, QWidget*> m_propertyWidgets;

    // Current asset
    AssetInfo m_currentAsset;
};

//===============================================================================
// Asset Search Widget
//===============================================================================

class AssetSearchWidget : public QWidget
{
    Q_OBJECT

public:
    AssetSearchWidget(QWidget *parent = nullptr);
    ~AssetSearchWidget() override;

    QString searchText() const;
    void setSearchText(const QString& text);

signals:
    void searchTextChanged(const QString& text);
    void searchRequested(const QString& text);

private:
    void setupUI();

    QHBoxLayout *m_layout = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_advancedButton = nullptr;
};