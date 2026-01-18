/*
===============================================================================

Qt Asset Browser Implementation

===============================================================================
*/

#include "asset_browser.h"
#include "../common/qcommon.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QProgressDialog>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPixmap>
#include <QUrl>
#include <algorithm>

//===============================================================================
// AssetBrowser
//===============================================================================

AssetBrowser::AssetBrowser(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();

    // Set default base path
    setBasePath(QDir::currentPath() + "/baseq3");

    // Initial scan
    refreshAssets();
}

AssetBrowser::~AssetBrowser()
{
}

void AssetBrowser::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Main splitter
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainLayout->addWidget(m_mainSplitter);

    // Left panel - directory tree
    createTreeView();
    m_mainSplitter->addWidget(m_treeGroup);

    // Right panel - assets and preview
    m_rightSplitter = new QSplitter(Qt::Vertical, nullptr);
    m_mainSplitter->addWidget(m_rightSplitter);

    // Asset view (top right)
    createAssetView();
    m_rightSplitter->addWidget(m_assetGroup);

    // Preview and properties (bottom right)
    QWidget *bottomPanel = new QWidget();
    QHBoxLayout *bottomLayout = new QHBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    createPreviewPanel();
    createPropertyPanel();

    bottomLayout->addWidget(m_previewGroup);
    bottomLayout->addWidget(m_propertyGroup);

    m_rightSplitter->addWidget(bottomPanel);

    // Set splitter proportions
    m_mainSplitter->setStretchFactor(0, 1);
    m_mainSplitter->setStretchFactor(1, 3);
    m_rightSplitter->setStretchFactor(0, 2);
    m_rightSplitter->setStretchFactor(1, 1);

    // Create context menu
    m_contextMenu = new QMenu(this);
    m_contextMenu->addAction("Open", this, [this]() { /* TODO */ });
    m_contextMenu->addAction("Open in Explorer", this, [this]() { /* TODO */ });
    m_contextMenu->addSeparator();
    m_contextMenu->addAction("Import...", this, &AssetBrowser::importAsset);
    m_contextMenu->addAction("Export...", this, &AssetBrowser::exportAsset);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction("Delete", this, &AssetBrowser::deleteAsset);
    m_contextMenu->addAction("Rename", this, &AssetBrowser::renameAsset);
    m_contextMenu->addAction("Duplicate", this, &AssetBrowser::duplicateAsset);
}

void AssetBrowser::createTreeView()
{
    m_treeGroup = new QGroupBox("Directories", this);
    QVBoxLayout *layout = new QVBoxLayout(m_treeGroup);

    m_directoryTree = new QTreeWidget(m_treeGroup);
    m_directoryTree->setHeaderLabel("Directory Structure");
    m_directoryTree->setMinimumWidth(200);
    layout->addWidget(m_directoryTree);

    // TODO: Populate directory tree
}

void AssetBrowser::createAssetView()
{
    m_assetGroup = new QGroupBox("Assets", this);
    m_assetLayout = new QVBoxLayout(m_assetGroup);

    // Toolbar
    createToolbar();
    m_assetLayout->addLayout(m_toolbarLayout);

    // Asset list
    m_assetList = new QListWidget(m_assetGroup);
    m_assetList->setViewMode(QListView::IconMode);
    m_assetList->setIconSize(QSize(64, 64));
    m_assetList->setGridSize(QSize(80, 80));
    m_assetList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_assetLayout->addWidget(m_assetList);
}

void AssetBrowser::createToolbar()
{
    m_toolbarLayout = new QHBoxLayout();

    m_backButton = new QPushButton("←", this);
    m_backButton->setMaximumWidth(30);
    m_toolbarLayout->addWidget(m_backButton);

    m_forwardButton = new QPushButton("→", this);
    m_forwardButton->setMaximumWidth(30);
    m_toolbarLayout->addWidget(m_forwardButton);

    m_upButton = new QPushButton("↑", this);
    m_upButton->setMaximumWidth(30);
    m_toolbarLayout->addWidget(m_upButton);

    m_pathLabel = new QLabel("/", this);
    m_pathLabel->setStyleSheet("font-weight: bold;");
    m_toolbarLayout->addWidget(m_pathLabel, 1);

    m_refreshButton = new QPushButton("Refresh", this);
    m_refreshButton->setMaximumWidth(70);
    m_toolbarLayout->addWidget(m_refreshButton);

    // Filter controls
    m_filterLayout = new QHBoxLayout();

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search assets...");
    m_searchEdit->setMaximumWidth(150);
    m_filterLayout->addWidget(m_searchEdit);

    m_typeFilterCombo = new QComboBox(this);
    m_typeFilterCombo->addItem("All Types");
    m_typeFilterCombo->addItem("Textures");
    m_typeFilterCombo->addItem("Models");
    m_typeFilterCombo->addItem("Sounds");
    m_typeFilterCombo->addItem("Shaders");
    m_typeFilterCombo->addItem("Materials");
    m_typeFilterCombo->addItem("Levels");
    m_typeFilterCombo->setMaximumWidth(100);
    m_filterLayout->addWidget(m_typeFilterCombo);

    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->addItem("Icons");
    m_viewModeCombo->addItem("List");
    m_viewModeCombo->addItem("Details");
    m_viewModeCombo->setMaximumWidth(80);
    m_filterLayout->addWidget(m_viewModeCombo);

    m_toolbarLayout->addLayout(m_filterLayout);
}

void AssetBrowser::createPreviewPanel()
{
    m_previewGroup = new QGroupBox("Preview", this);
    QVBoxLayout *layout = new QVBoxLayout(m_previewGroup);

    m_previewWidget = new AssetPreviewWidget(m_previewGroup);
    layout->addWidget(m_previewWidget);
}

void AssetBrowser::createPropertyPanel()
{
    m_propertyGroup = new QGroupBox("Properties", this);
    QVBoxLayout *layout = new QVBoxLayout(m_propertyGroup);

    m_propertyEditor = new AssetPropertyEditor(m_propertyGroup);
    layout->addWidget(m_propertyEditor);
}

void AssetBrowser::setupConnections()
{
    connect(m_backButton, &QPushButton::clicked, this, &AssetBrowser::goBack);
    connect(m_forwardButton, &QPushButton::clicked, this, &AssetBrowser::goForward);
    connect(m_upButton, &QPushButton::clicked, this, &AssetBrowser::goUp);
    connect(m_refreshButton, &QPushButton::clicked, this, &AssetBrowser::onRefreshClicked);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &AssetBrowser::onSearchTextChanged);
    connect(m_typeFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AssetBrowser::onFilterTypeChanged);

    connect(m_assetList, &QListWidget::itemClicked, this, &AssetBrowser::onAssetItemClicked);
    connect(m_assetList, &QListWidget::itemDoubleClicked, this, &AssetBrowser::onAssetItemDoubleClicked);
    connect(m_assetList, &QListWidget::customContextMenuRequested, this, &AssetBrowser::onAssetContextMenu);

    connect(&m_scanTimer, &QTimer::timeout, this, [this]() {
        if (m_isScanning) {
            updateAssetList();
        }
    });
}

void AssetBrowser::setBasePath(const QString& path)
{
    m_basePath = QDir::cleanPath(path);
    navigateToPath(m_basePath);
}

void AssetBrowser::refreshAssets()
{
    if (m_isScanning) return;

    m_isScanning = true;
    m_allAssets.clear();

    // Start scanning in background
    QTimer::singleShot(0, this, [this]() {
        scanDirectory(m_currentPath);
        m_isScanning = false;
        updateAssetList();
        emit assetsRefreshed();
    });
}

void AssetBrowser::scanDirectory(const QString& path)
{
    QDir dir(path);
    if (!dir.exists()) return;

    scanDirectoryRecursive(path, m_basePath);
}

void AssetBrowser::scanDirectoryRecursive(const QString& path, const QString& basePath)
{
    QDir dir(path);
    QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo& info : entries) {
        if (info.isDir()) {
            // Recurse into subdirectories
            scanDirectoryRecursive(info.absoluteFilePath(), basePath);
        } else {
            // Create asset info
            AssetInfo asset = createAssetInfo(info, basePath);
            if (asset.type != AssetType::Unknown) {
                m_allAssets.push_back(asset);
            }
        }
    }
}

AssetInfo AssetBrowser::createAssetInfo(const QFileInfo& fileInfo, const QString& basePath)
{
    AssetInfo asset;
    asset.name = fileInfo.baseName();
    asset.fullPath = fileInfo.absoluteFilePath();
    asset.path = QDir(basePath).relativeFilePath(fileInfo.absoluteFilePath());
    asset.size = fileInfo.size();
    asset.modified = fileInfo.lastModified();
    asset.type = detectAssetType(fileInfo.fileName());

    // Basic metadata
    asset.metadata["extension"] = fileInfo.suffix().toStdString();
    asset.metadata["absolute_path"] = fileInfo.absoluteFilePath().toStdString();

    return asset;
}

AssetType AssetBrowser::detectAssetType(const QString& filename)
{
    QString ext = QFileInfo(filename).suffix().toLower();

    if (ext == "tga" || ext == "png" || ext == "jpg" || ext == "jpeg" ||
        ext == "bmp" || ext == "ktx2" || ext == "dds") {
        return AssetType::Texture;
    }

    if (ext == "md3" || ext == "md5" || ext == "obj" || ext == "fbx" ||
        ext == "dae" || ext == "iqm" || ext == "gltf" || ext == "glb") {
        return AssetType::Model;
    }

    if (ext == "wav" || ext == "ogg" || ext == "mp3" || ext == "flac") {
        return AssetType::Sound;
    }

    if (ext == "shader") {
        return AssetType::Shader;
    }

    if (ext == "material") {
        return AssetType::Material;
    }

    if (ext == "bsp" || ext == "map") {
        return AssetType::Level;
    }

    if (ext == "cfg" || ext == "config") {
        return AssetType::Config;
    }

    if (ext == "pk3" || ext == "pk4" || ext == "orb") {
        return AssetType::PakFile;
    }

    return AssetType::Unknown;
}

void AssetBrowser::updateAssetList()
{
    m_assetList->clear();
    m_filteredAssets.clear();

    // Apply filters
    for (const AssetInfo& asset : m_allAssets) {
        bool matchesType = (m_filterType == AssetType::Unknown) ||
                          (asset.type == m_filterType);

        bool matchesSearch = m_searchText.isEmpty() ||
                           asset.name.contains(m_searchText, Qt::CaseInsensitive) ||
                           asset.path.contains(m_searchText, Qt::CaseInsensitive);

        if (matchesType && matchesSearch) {
            m_filteredAssets.push_back(asset);

            // Add to list widget
            QListWidgetItem *item = new QListWidgetItem(m_assetList);
            item->setText(asset.name);
            item->setIcon(getAssetIcon(asset.type));
            item->setToolTip(QString("%1\nSize: %2\nModified: %3")
                           .arg(asset.path)
                           .arg(formatFileSize(asset.size))
                           .arg(asset.modified.toString()));

            // Store asset info in item data
            item->setData(Qt::UserRole, QVariant::fromValue(asset));
        }
    }

    m_assetList->sortItems();
}

void AssetBrowser::updatePreview(const AssetInfo& asset)
{
    if (m_previewWidget) {
        m_previewWidget->setAsset(asset);
    }
}

void AssetBrowser::updateProperties(const AssetInfo& asset)
{
    if (m_propertyEditor) {
        m_propertyEditor->setAsset(asset);
    }
}

QIcon AssetBrowser::getAssetIcon(AssetType type) const
{
    // TODO: Load actual icons from resources
    switch (type) {
        case AssetType::Texture: return QIcon(":/icons/texture.png");
        case AssetType::Model: return QIcon(":/icons/model.png");
        case AssetType::Sound: return QIcon(":/icons/sound.png");
        case AssetType::Shader: return QIcon(":/icons/shader.png");
        case AssetType::Material: return QIcon(":/icons/material.png");
        case AssetType::Level: return QIcon(":/icons/level.png");
        default: return QIcon(":/icons/file.png");
    }
}

QString AssetBrowser::getAssetTypeName(AssetType type) const
{
    switch (type) {
        case AssetType::Texture: return "Texture";
        case AssetType::Model: return "Model";
        case AssetType::Sound: return "Sound";
        case AssetType::Shader: return "Shader";
        case AssetType::Material: return "Material";
        case AssetType::Animation: return "Animation";
        case AssetType::Level: return "Level";
        case AssetType::Script: return "Script";
        case AssetType::Config: return "Config";
        case AssetType::PakFile: return "PAK File";
        default: return "Unknown";
    }
}

QString AssetBrowser::formatFileSize(qint64 size) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double fileSize = size;

    while (fileSize >= 1024 && unitIndex < 4) {
        fileSize /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(fileSize, 0, 'f', 1).arg(units[unitIndex]);
}

AssetInfo AssetBrowser::selectedAsset() const
{
    QList<QListWidgetItem*> selected = m_assetList->selectedItems();
    if (selected.isEmpty()) {
        return AssetInfo();
    }

    return selected.first()->data(Qt::UserRole).value<AssetInfo>();
}

QList<AssetInfo> AssetBrowser::selectedAssets() const
{
    QList<AssetInfo> assets;
    QList<QListWidgetItem*> selected = m_assetList->selectedItems();

    for (QListWidgetItem* item : selected) {
        assets.append(item->data(Qt::UserRole).value<AssetInfo>());
    }

    return assets;
}

void AssetBrowser::setFilterType(AssetType type)
{
    m_filterType = type;
    updateAssetList();
}

void AssetBrowser::setFilterText(const QString& text)
{
    m_searchText = text;
    updateAssetList();
}

void AssetBrowser::navigateToPath(const QString& path)
{
    m_currentPath = QDir::cleanPath(path);
    m_pathLabel->setText(m_currentPath);
    refreshAssets();
    emit directoryChanged(m_currentPath);
}

void AssetBrowser::goUp()
{
    QDir dir(m_currentPath);
    if (dir.cdUp()) {
        navigateToPath(dir.absolutePath());
    }
}

void AssetBrowser::goBack()
{
    // TODO: Implement navigation history
}

void AssetBrowser::goForward()
{
    // TODO: Implement navigation history
}

void AssetBrowser::importAsset()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Import Asset",
                                                   m_currentPath,
                                                   "All files (*.*)");
    if (!fileName.isEmpty()) {
        // TODO: Import asset
        refreshAssets();
    }
}

void AssetBrowser::exportAsset()
{
    AssetInfo asset = selectedAsset();
    if (asset.fullPath.isEmpty()) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Export Asset",
                                                   asset.name,
                                                   "All files (*.*)");
    if (!fileName.isEmpty()) {
        // TODO: Export asset
    }
}

void AssetBrowser::deleteAsset()
{
    QList<AssetInfo> assets = selectedAssets();
    if (assets.isEmpty()) return;

    QString message = assets.size() == 1 ?
        QString("Delete asset '%1'?").arg(assets[0].name) :
        QString("Delete %1 assets?").arg(assets.size());

    QMessageBox::StandardButton result = QMessageBox::question(
        this, "Delete Assets", message,
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        // TODO: Delete assets
        refreshAssets();
    }
}

void AssetBrowser::renameAsset()
{
    AssetInfo asset = selectedAsset();
    if (asset.fullPath.isEmpty()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, "Rename Asset",
                                           "New name:", QLineEdit::Normal,
                                           asset.name, &ok);
    if (ok && !newName.isEmpty() && newName != asset.name) {
        // TODO: Rename asset
        refreshAssets();
    }
}

void AssetBrowser::duplicateAsset()
{
    AssetInfo asset = selectedAsset();
    if (asset.fullPath.isEmpty()) return;

    // TODO: Duplicate asset
    refreshAssets();
}

void AssetBrowser::setViewMode(int mode)
{
    m_viewMode = mode;

    switch (mode) {
        case 0: // Icons
            m_assetList->setViewMode(QListView::IconMode);
            break;
        case 1: // List
            m_assetList->setViewMode(QListView::ListMode);
            break;
        case 2: // Details
            // TODO: Switch to table view
            break;
    }
}

void AssetBrowser::onAssetItemClicked(QListWidgetItem* item)
{
    AssetInfo asset = item->data(Qt::UserRole).value<AssetInfo>();
    updatePreview(asset);
    updateProperties(asset);
    emit assetSelected(asset);
}

void AssetBrowser::onAssetItemDoubleClicked(QListWidgetItem* item)
{
    AssetInfo asset = item->data(Qt::UserRole).value<AssetInfo>();
    emit assetDoubleClicked(asset);
}

void AssetBrowser::onAssetContextMenu(const QPoint& pos)
{
    if (m_contextMenu) {
        m_contextMenu->exec(m_assetList->mapToGlobal(pos));
    }
}

void AssetBrowser::onDirectoryChanged(const QString& path)
{
    m_currentPath = path;
    refreshAssets();
}

void AssetBrowser::onSearchTextChanged(const QString& text)
{
    setFilterText(text);
}

void AssetBrowser::onFilterTypeChanged(int index)
{
    AssetType type = AssetType::Unknown;
    switch (index) {
        case 1: type = AssetType::Texture; break;
        case 2: type = AssetType::Model; break;
        case 3: type = AssetType::Sound; break;
        case 4: type = AssetType::Shader; break;
        case 5: type = AssetType::Material; break;
        case 6: type = AssetType::Level; break;
    }
    setFilterType(type);
}

void AssetBrowser::onRefreshClicked()
{
    refreshAssets();
}

//===============================================================================
// AssetPreviewWidget
//===============================================================================

AssetPreviewWidget::AssetPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

AssetPreviewWidget::~AssetPreviewWidget()
{
}

void AssetPreviewWidget::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(5);

    m_titleLabel = new QLabel("No asset selected", this);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 12px;");
    m_layout->addWidget(m_titleLabel);

    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(200, 150);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("border: 1px solid #ccc; background-color: #f0f0f0;");
    m_layout->addWidget(m_previewLabel, 1);

    m_infoText = new QTextEdit(this);
    m_infoText->setMaximumHeight(100);
    m_infoText->setReadOnly(true);
    m_layout->addWidget(m_infoText);
}

void AssetPreviewWidget::setAsset(const AssetInfo& asset)
{
    m_currentAsset = asset;

    m_titleLabel->setText(asset.name);

    // Update preview based on asset type
    switch (asset.type) {
        case AssetType::Texture:
            updateTexturePreview(asset);
            break;
        case AssetType::Model:
            updateModelPreview(asset);
            break;
        case AssetType::Sound:
            updateSoundPreview(asset);
            break;
        default:
            updateTextPreview(asset);
            break;
    }

    // Update info text
    QString info = QString("Type: %1\nSize: %2\nModified: %3\nPath: %4")
                  .arg(getAssetTypeName(asset.type))
                  .arg(formatFileSize(asset.size))
                  .arg(asset.modified.toString())
                  .arg(asset.path);
    m_infoText->setPlainText(info);
}

void AssetPreviewWidget::clearPreview()
{
    m_titleLabel->setText("No asset selected");
    m_previewLabel->clear();
    m_infoText->clear();
}

void AssetPreviewWidget::updateTexturePreview(const AssetInfo& asset)
{
    // TODO: Load and display texture preview
    m_previewLabel->setText("Texture Preview\n(Not implemented)");
}

void AssetPreviewWidget::updateModelPreview(const AssetInfo& asset)
{
    // TODO: Load and display 3D model preview
    m_previewLabel->setText("Model Preview\n(Not implemented)");
}

void AssetPreviewWidget::updateSoundPreview(const AssetInfo& asset)
{
    // TODO: Show sound waveform and play controls
    m_previewLabel->setText("Sound Preview\n(Not implemented)");
}

void AssetPreviewWidget::updateTextPreview(const AssetInfo& asset)
{
    // Show file contents preview
    QFile file(asset.fullPath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.read(1024)); // First 1KB
        if (content.length() == 1024) {
            content += "\n... (truncated)";
        }
        m_previewLabel->setText(content);
        file.close();
    } else {
        m_previewLabel->setText("Cannot preview file");
    }
}

QString AssetPreviewWidget::getAssetTypeName(AssetType type) const
{
    switch (type) {
        case AssetType::Texture: return "Texture";
        case AssetType::Model: return "Model";
        case AssetType::Sound: return "Sound";
        case AssetType::Shader: return "Shader";
        case AssetType::Material: return "Material";
        case AssetType::Level: return "Level";
        default: return "Unknown";
    }
}

QString AssetPreviewWidget::formatFileSize(qint64 size) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double fileSize = size;

    while (fileSize >= 1024 && unitIndex < 4) {
        fileSize /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(fileSize, 0, 'f', 1).arg(units[unitIndex]);
}

//===============================================================================
// AssetPropertyEditor
//===============================================================================

AssetPropertyEditor::AssetPropertyEditor(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

AssetPropertyEditor::~AssetPropertyEditor()
{
}

void AssetPropertyEditor::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(5, 5, 5, 5);
    m_layout->setSpacing(5);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    m_contentWidget = new QWidget();
    m_contentLayout = new QVBoxLayout(m_contentWidget);
    m_contentLayout->setSpacing(2);

    m_scrollArea->setWidget(m_contentWidget);
    m_layout->addWidget(m_scrollArea);
}

void AssetPropertyEditor::setAsset(const AssetInfo& asset)
{
    m_currentAsset = asset;
    clearProperties();
    createBasicProperties(asset);
    createTypeSpecificProperties(asset);
}

void AssetPropertyEditor::clearProperties()
{
    // Clear existing widgets
    QLayoutItem *child;
    while ((child = m_contentLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    m_propertyWidgets.clear();
}

void AssetPropertyEditor::createBasicProperties(const AssetInfo& asset)
{
    // Name
    QLabel *nameLabel = new QLabel("Name:", m_contentWidget);
    QLineEdit *nameEdit = new QLineEdit(asset.name, m_contentWidget);
    nameEdit->setReadOnly(true);
    m_contentLayout->addWidget(nameLabel);
    m_contentLayout->addWidget(nameEdit);

    // Path
    QLabel *pathLabel = new QLabel("Path:", m_contentWidget);
    QLineEdit *pathEdit = new QLineEdit(asset.path, m_contentWidget);
    pathEdit->setReadOnly(true);
    m_contentLayout->addWidget(pathLabel);
    m_contentLayout->addWidget(pathEdit);

    // Size
    QLabel *sizeLabel = new QLabel("Size:", m_contentWidget);
    QLabel *sizeValueLabel = new QLabel(formatFileSize(asset.size), m_contentWidget);
    m_contentLayout->addWidget(sizeLabel);
    m_contentLayout->addWidget(sizeValueLabel);

    // Modified
    QLabel *modifiedLabel = new QLabel("Modified:", m_contentWidget);
    QLabel *modifiedValueLabel = new QLabel(asset.modified.toString(), m_contentWidget);
    m_contentLayout->addWidget(modifiedLabel);
    m_contentLayout->addWidget(modifiedValueLabel);

    // Type
    QLabel *typeLabel = new QLabel("Type:", m_contentWidget);
    QLabel *typeValueLabel = new QLabel(getAssetTypeName(asset.type), m_contentWidget);
    m_contentLayout->addWidget(typeLabel);
    m_contentLayout->addWidget(typeValueLabel);
}

void AssetPropertyEditor::createTypeSpecificProperties(const AssetInfo& asset)
{
    switch (asset.type) {
        case AssetType::Texture:
            // TODO: Add texture-specific properties (width, height, format, etc.)
            break;
        case AssetType::Model:
            // TODO: Add model-specific properties (vertices, faces, animations, etc.)
            break;
        case AssetType::Sound:
            // TODO: Add sound-specific properties (duration, sample rate, channels, etc.)
            break;
        default:
            break;
    }
}

void AssetPropertyEditor::onPropertyValueChanged()
{
    // TODO: Handle property changes
}

QString AssetPropertyEditor::getAssetTypeName(AssetType type) const
{
    switch (type) {
        case AssetType::Texture: return "Texture";
        case AssetType::Model: return "Model";
        case AssetType::Sound: return "Sound";
        case AssetType::Shader: return "Shader";
        case AssetType::Material: return "Material";
        case AssetType::Level: return "Level";
        default: return "Unknown";
    }
}

QString AssetPropertyEditor::formatFileSize(qint64 size) const
{
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double fileSize = size;

    while (fileSize >= 1024 && unitIndex < 4) {
        fileSize /= 1024;
        unitIndex++;
    }

    return QString("%1 %2").arg(fileSize, 0, 'f', 1).arg(units[unitIndex]);
}