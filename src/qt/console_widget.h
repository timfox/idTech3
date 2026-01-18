/*
===============================================================================

Qt Console Widget for id Tech 3

Provides a Qt-based console interface with syntax highlighting, command completion,
and history.

===============================================================================
*/

#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QScrollBar>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <memory>
#include <deque>
#include <vector>
#include <string>

class ConsoleHighlighter;

// Console widget class
class ConsoleWidget : public QWidget
{
    Q_OBJECT

public:
    ConsoleWidget(QWidget *parent = nullptr);
    ~ConsoleWidget() override;

    // Console control
    void print(const QString& text);
    void clear();
    void scrollToBottom();

    // Command history
    void addToHistory(const QString& command);
    QString getHistoryItem(int index) const;
    int historySize() const { return m_commandHistory.size(); }

    // Auto-completion
    void setAutoCompleteList(const QStringList& commands);
    void showAutoComplete();

    // Settings
    void setMaxLines(int maxLines);
    int maxLines() const { return m_maxLines; }

    void setFontSize(int size);
    int fontSize() const { return m_fontSize; }

    void setWordWrap(bool wrap);
    bool wordWrap() const { return m_wordWrap; }

signals:
    // Emitted when user enters a command
    void commandEntered(const QString& command);

    // Emitted when console output changes
    void outputChanged();

public slots:
    // Execute a command
    void executeCommand(const QString& command);

    // Clear console output
    void clearOutput();

    // Save console log to file
    void saveLog();

private slots:
    void onCommandEntered();
    void onTextChanged();
    void onContextMenuRequested(const QPoint& pos);
    void updateAutoComplete();
    void insertAutoCompleteText();

private:
    void setupUI();
    void setupConnections();
    void createContextMenu();
    void applySettings();

    // Command processing
    void processCommand(const QString& command);
    QStringList parseCommandLine(const QString& command);

    // History navigation
    void historyUp();
    void historyDown();

    // Auto-completion
    QString findCommonPrefix(const QStringList& candidates);
    QStringList getCompletionCandidates(const QString& prefix);

    // UI components
    QVBoxLayout *m_mainLayout = nullptr;
    QTextEdit *m_outputText = nullptr;
    QLineEdit *m_commandInput = nullptr;
    QHBoxLayout *m_inputLayout = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    QComboBox *m_filterCombo = nullptr;

    // Context menu
    QMenu *m_contextMenu = nullptr;

    // Syntax highlighter
    std::unique_ptr<ConsoleHighlighter> m_highlighter;

    // Command history
    std::deque<QString> m_commandHistory;
    int m_historyIndex = -1;
    static constexpr int MAX_HISTORY_SIZE = 1000;

    // Auto-completion
    QStringList m_autoCompleteList;
    QString m_currentCompletionPrefix;
    int m_completionIndex = -1;

    // Settings
    int m_maxLines = 10000;
    int m_fontSize = 10;
    bool m_wordWrap = false;

    // Output buffering
    QString m_pendingOutput;
    QTimer m_outputTimer;

protected:
    // Event handling
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

// Syntax highlighter for console output
class ConsoleHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    ConsoleHighlighter(QTextDocument *parent = nullptr);
    ~ConsoleHighlighter() override;

protected:
    void highlightBlock(const QString &text) override;

private:
    // Highlighting rules
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> m_highlightingRules;

    // Formats for different types of text
    QTextCharFormat m_errorFormat;
    QTextCharFormat m_warningFormat;
    QTextCharFormat m_infoFormat;
    QTextCharFormat m_commandFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;

    void setupFormats();
    void setupRules();
};