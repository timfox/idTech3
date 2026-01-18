/*
===============================================================================

Qt Console Widget Implementation

===============================================================================
*/

#include "console_widget.h"
#include "../common/qcommon.h"

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QTextStream>
#include <QTextBlock>
#include <QTextCursor>
#include <QScrollBar>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <algorithm>
#include <ctime>

//===============================================================================
// ConsoleWidget
//===============================================================================

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
    setupConnections();
    createContextMenu();
    applySettings();

    // Set up output buffering timer
    m_outputTimer.setInterval(100); // 100ms buffer
    m_outputTimer.setSingleShot(true);
    connect(&m_outputTimer, &QTimer::timeout, this, [this]() {
        if (!m_pendingOutput.isEmpty()) {
            m_outputText->append(m_pendingOutput);
            m_pendingOutput.clear();
            scrollToBottom();
            emit outputChanged();
        }
    });

    // Install event filter for command input
    m_commandInput->installEventFilter(this);
}

ConsoleWidget::~ConsoleWidget()
{
}

void ConsoleWidget::setupUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);
    m_mainLayout->setSpacing(5);

    // Output text area
    m_outputText = new QTextEdit(this);
    m_outputText->setReadOnly(true);
    m_outputText->setFont(QFont("Courier New", m_fontSize));
    m_outputText->setWordWrapMode(m_wordWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap);
    m_outputText->setContextMenuPolicy(Qt::CustomContextMenu);
    m_mainLayout->addWidget(m_outputText, 1);

    // Input layout
    m_inputLayout = new QHBoxLayout();
    m_inputLayout->setSpacing(5);

    // Command input
    m_commandInput = new QLineEdit(this);
    m_commandInput->setFont(QFont("Courier New", m_fontSize));
    m_commandInput->setPlaceholderText("Enter console command...");
    m_inputLayout->addWidget(m_commandInput, 1);

    // Control buttons
    m_clearButton = new QPushButton("Clear", this);
    m_clearButton->setMaximumWidth(60);
    m_inputLayout->addWidget(m_clearButton);

    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setMaximumWidth(60);
    m_inputLayout->addWidget(m_saveButton);

    // Filter combo box
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem("All");
    m_filterCombo->addItem("Errors");
    m_filterCombo->addItem("Warnings");
    m_filterCombo->addItem("Info");
    m_filterCombo->setMaximumWidth(80);
    m_inputLayout->addWidget(m_filterCombo);

    m_mainLayout->addLayout(m_inputLayout);

    // Create syntax highlighter
    m_highlighter = std::make_unique<ConsoleHighlighter>(m_outputText->document());
}

void ConsoleWidget::setupConnections()
{
    connect(m_commandInput, &QLineEdit::returnPressed, this, &ConsoleWidget::onCommandEntered);
    connect(m_clearButton, &QPushButton::clicked, this, &ConsoleWidget::clearOutput);
    connect(m_saveButton, &QPushButton::clicked, this, &ConsoleWidget::saveLog);
    connect(m_outputText, &QTextEdit::customContextMenuRequested, this, &ConsoleWidget::onContextMenuRequested);
    connect(m_outputText, &QTextEdit::textChanged, this, &ConsoleWidget::onTextChanged);
    connect(m_commandInput, &QLineEdit::textChanged, this, &ConsoleWidget::updateAutoComplete);
}

void ConsoleWidget::createContextMenu()
{
    m_contextMenu = new QMenu(this);

    QAction *copyAction = m_contextMenu->addAction("Copy");
    QAction *selectAllAction = m_contextMenu->addAction("Select All");
    m_contextMenu->addSeparator();
    QAction *clearAction = m_contextMenu->addAction("Clear");
    QAction *saveAction = m_contextMenu->addAction("Save Log...");

    connect(copyAction, &QAction::triggered, [this]() {
        m_outputText->copy();
    });

    connect(selectAllAction, &QAction::triggered, [this]() {
        m_outputText->selectAll();
    });

    connect(clearAction, &QAction::triggered, this, &ConsoleWidget::clearOutput);
    connect(saveAction, &QAction::triggered, this, &ConsoleWidget::saveLog);
}

void ConsoleWidget::applySettings()
{
    QFont font("Courier New", m_fontSize);
    m_outputText->setFont(font);
    m_commandInput->setFont(font);

    m_outputText->setWordWrapMode(m_wordWrap ? QTextOption::WrapAnywhere : QTextOption::NoWrap);

    // Limit maximum lines
    while (m_outputText->document()->blockCount() > m_maxLines) {
        QTextCursor cursor(m_outputText->document()->firstBlock());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar(); // Remove the newline
    }
}

void ConsoleWidget::print(const QString& text)
{
    if (text.isEmpty()) return;

    // Buffer output for performance
    m_pendingOutput += text;

    // If this is a complete line (ends with newline), flush immediately
    if (text.endsWith('\n')) {
        m_outputTimer.stop();
        if (!m_pendingOutput.isEmpty()) {
            m_outputText->append(m_pendingOutput);
            m_pendingOutput.clear();
            scrollToBottom();
            emit outputChanged();
        }
    } else {
        // Start buffering timer
        if (!m_outputTimer.isActive()) {
            m_outputTimer.start();
        }
    }
}

void ConsoleWidget::clear()
{
    m_outputText->clear();
    m_pendingOutput.clear();
    emit outputChanged();
}

void ConsoleWidget::scrollToBottom()
{
    QScrollBar *scrollBar = m_outputText->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void ConsoleWidget::addToHistory(const QString& command)
{
    if (command.isEmpty()) return;

    // Remove duplicate if it exists
    m_commandHistory.erase(
        std::remove(m_commandHistory.begin(), m_commandHistory.end(), command),
        m_commandHistory.end());

    // Add to front
    m_commandHistory.push_front(command);

    // Limit history size
    while (m_commandHistory.size() > MAX_HISTORY_SIZE) {
        m_commandHistory.pop_back();
    }

    m_historyIndex = -1;
}

QString ConsoleWidget::getHistoryItem(int index) const
{
    if (index < 0 || index >= (int)m_commandHistory.size()) {
        return QString();
    }
    return m_commandHistory[index];
}

void ConsoleWidget::setAutoCompleteList(const QStringList& commands)
{
    m_autoCompleteList = commands;
    std::sort(m_autoCompleteList.begin(), m_autoCompleteList.end());
}

void ConsoleWidget::showAutoComplete()
{
    QString currentText = m_commandInput->text();
    if (currentText.isEmpty()) return;

    QStringList candidates = getCompletionCandidates(currentText);
    if (candidates.isEmpty()) return;

    if (candidates.size() == 1) {
        // Single completion
        m_commandInput->setText(candidates[0]);
    } else {
        // Multiple completions - show common prefix
        QString commonPrefix = findCommonPrefix(candidates);
        if (commonPrefix.length() > currentText.length()) {
            m_commandInput->setText(commonPrefix);
        }

        // Print completions to console
        print("Completions:\n");
        for (const QString& candidate : candidates) {
            print("  " + candidate + "\n");
        }
    }
}

void ConsoleWidget::setMaxLines(int maxLines)
{
    m_maxLines = std::max(100, maxLines);
    applySettings();
}

void ConsoleWidget::setFontSize(int size)
{
    m_fontSize = std::max(8, std::min(24, size));
    applySettings();
}

void ConsoleWidget::setWordWrap(bool wrap)
{
    m_wordWrap = wrap;
    applySettings();
}

void ConsoleWidget::executeCommand(const QString& command)
{
    if (command.isEmpty()) return;

    print("] " + command + "\n");

    // Add to history
    addToHistory(command);

    // Emit signal
    emit commandEntered(command);

    // Clear input
    m_commandInput->clear();
}

void ConsoleWidget::clearOutput()
{
    clear();
}

void ConsoleWidget::saveLog()
{
    QString fileName = QFileDialog::getSaveFileName(this, "Save Console Log",
                                                   "console.log",
                                                   "Log files (*.log *.txt);;All files (*)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << m_outputText->toPlainText();
        file.close();
    } else {
        print("Error: Failed to save console log to " + fileName + "\n");
    }
}

void ConsoleWidget::onCommandEntered()
{
    QString command = m_commandInput->text().trimmed();
    if (!command.isEmpty()) {
        executeCommand(command);
    }
}

void ConsoleWidget::onTextChanged()
{
    applySettings();
}

void ConsoleWidget::onContextMenuRequested(const QPoint& pos)
{
    if (m_contextMenu) {
        m_contextMenu->exec(m_outputText->mapToGlobal(pos));
    }
}

void ConsoleWidget::updateAutoComplete()
{
    QString currentText = m_commandInput->text();
    if (currentText.isEmpty()) {
        m_completionIndex = -1;
        return;
    }

    // Check if Tab was pressed for completion
    // This is handled in keyPressEvent
}

void ConsoleWidget::insertAutoCompleteText()
{
    if (m_completionIndex >= 0 && m_completionIndex < m_autoCompleteList.size()) {
        m_commandInput->setText(m_autoCompleteList[m_completionIndex]);
        m_completionIndex = -1;
    }
}

void ConsoleWidget::processCommand(const QString& command)
{
    // Parse command line
    QStringList args = parseCommandLine(command);

    if (args.isEmpty()) return;

    QString cmd = args[0].toLower();

    // Handle built-in console commands
    if (cmd == "clear") {
        clearOutput();
    } else if (cmd == "help") {
        print("Available console commands:\n");
        print("  clear          - Clear console output\n");
        print("  help           - Show this help\n");
        print("  history        - Show command history\n");
        print("  save <file>    - Save console log to file\n");
        print("  echo <text>    - Echo text to console\n");
    } else if (cmd == "history") {
        print("Command history:\n");
        for (int i = 0; i < historySize(); ++i) {
            print(QString("  %1: %2\n").arg(i + 1, 3, 10, QChar('0')).arg(getHistoryItem(i)));
        }
    } else if (cmd == "save" && args.size() > 1) {
        // Save to specified file
        QFile file(args[1]);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << m_outputText->toPlainText();
            file.close();
            print("Console log saved to " + args[1] + "\n");
        } else {
            print("Error: Failed to save console log to " + args[1] + "\n");
        }
    } else if (cmd == "echo") {
        QString text = command.mid(5); // Skip "echo "
        print(text + "\n");
    } else {
        // Pass to game console system
        executeCommand(command);
    }
}

QStringList ConsoleWidget::parseCommandLine(const QString& command)
{
    QStringList args;
    QString current;
    bool inQuotes = false;
    QChar quoteChar;

    for (int i = 0; i < command.length(); ++i) {
        QChar c = command[i];

        if (inQuotes) {
            if (c == quoteChar) {
                inQuotes = false;
            } else {
                current += c;
            }
        } else {
            if (c == '"' || c == '\'') {
                inQuotes = true;
                quoteChar = c;
            } else if (c.isSpace()) {
                if (!current.isEmpty()) {
                    args.append(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
    }

    if (!current.isEmpty()) {
        args.append(current);
    }

    return args;
}

void ConsoleWidget::historyUp()
{
    if (m_commandHistory.empty()) return;

    if (m_historyIndex < (int)m_commandHistory.size() - 1) {
        m_historyIndex++;
        m_commandInput->setText(m_commandHistory[m_historyIndex]);
    }
}

void ConsoleWidget::historyDown()
{
    if (m_historyIndex > 0) {
        m_historyIndex--;
        m_commandInput->setText(m_commandHistory[m_historyIndex]);
    } else if (m_historyIndex == 0) {
        m_historyIndex = -1;
        m_commandInput->setText(QString());
    }
}

QString ConsoleWidget::findCommonPrefix(const QStringList& candidates)
{
    if (candidates.isEmpty()) return QString();

    QString prefix = candidates[0];
    for (int i = 1; i < candidates.size(); ++i) {
        const QString& candidate = candidates[i];
        int len = std::min(prefix.length(), candidate.length());
        int j = 0;
        for (; j < len; ++j) {
            if (prefix[j] != candidate[j]) break;
        }
        prefix = prefix.left(j);
        if (prefix.isEmpty()) break;
    }

    return prefix;
}

QStringList ConsoleWidget::getCompletionCandidates(const QString& prefix)
{
    QStringList candidates;

    for (const QString& cmd : m_autoCompleteList) {
        if (cmd.startsWith(prefix, Qt::CaseInsensitive)) {
            candidates.append(cmd);
        }
    }

    return candidates;
}

void ConsoleWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Up:
            historyUp();
            event->accept();
            return;

        case Qt::Key_Down:
            historyDown();
            event->accept();
            return;

        case Qt::Key_Tab:
            showAutoComplete();
            event->accept();
            return;

        case Qt::Key_PageUp:
            // Scroll output up
            {
                QScrollBar *scrollBar = m_outputText->verticalScrollBar();
                scrollBar->setValue(scrollBar->value() - scrollBar->pageStep());
            }
            event->accept();
            return;

        case Qt::Key_PageDown:
            // Scroll output down
            {
                QScrollBar *scrollBar = m_outputText->verticalScrollBar();
                scrollBar->setValue(scrollBar->value() + scrollBar->pageStep());
            }
            event->accept();
            return;

        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void ConsoleWidget::wheelEvent(QWheelEvent *event)
{
    // Forward wheel events to output text area for scrolling
    if (m_outputText->rect().contains(event->pos())) {
        QApplication::sendEvent(m_outputText, event);
    } else {
        QWidget::wheelEvent(event);
    }
}

bool ConsoleWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_commandInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        keyPressEvent(keyEvent);
        if (keyEvent->isAccepted()) {
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

//===============================================================================
// ConsoleHighlighter
//===============================================================================

ConsoleHighlighter::ConsoleHighlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    setupFormats();
    setupRules();
}

ConsoleHighlighter::~ConsoleHighlighter()
{
}

void ConsoleHighlighter::setupFormats()
{
    // Error format (red)
    m_errorFormat.setForeground(Qt::red);
    m_errorFormat.setFontWeight(QFont::Bold);

    // Warning format (yellow/orange)
    m_warningFormat.setForeground(QColor(255, 165, 0)); // Orange
    m_warningFormat.setFontWeight(QFont::Bold);

    // Info format (blue)
    m_infoFormat.setForeground(Qt::blue);

    // Command format (green)
    m_commandFormat.setForeground(Qt::darkGreen);

    // Number format (magenta)
    m_numberFormat.setForeground(Qt::magenta);

    // String format (dark red)
    m_stringFormat.setForeground(Qt::darkRed);

    // Comment format (gray)
    m_commentFormat.setForeground(Qt::gray);
    m_commentFormat.setFontItalic(true);
}

void ConsoleHighlighter::setupRules()
{
    // Error patterns
    HighlightingRule errorRule;
    errorRule.pattern = QRegularExpression("\\b(ERROR|Error|error)\\b");
    errorRule.format = m_errorFormat;
    m_highlightingRules.append(errorRule);

    // Warning patterns
    HighlightingRule warningRule;
    warningRule.pattern = QRegularExpression("\\b(WARNING|Warning|warning|WARN)\\b");
    warningRule.format = m_warningFormat;
    m_highlightingRules.append(warningRule);

    // Info patterns
    HighlightingRule infoRule;
    infoRule.pattern = QRegularExpression("\\b(INFO|Info|info)\\b");
    infoRule.format = m_infoFormat;
    m_highlightingRules.append(infoRule);

    // Command prompt
    HighlightingRule commandRule;
    commandRule.pattern = QRegularExpression("^\\]");
    commandRule.format = m_commandFormat;
    m_highlightingRules.append(commandRule);

    // Numbers
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b\\d+\\.?\\d*\\b");
    numberRule.format = m_numberFormat;
    m_highlightingRules.append(numberRule);

    // Strings (quoted)
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\"[^\"]*\"|'[^']*'");
    stringRule.format = m_stringFormat;
    m_highlightingRules.append(stringRule);

    // Comments (starting with # or //)
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression("(//.*|#.*)$");
    commentRule.format = m_commentFormat;
    m_highlightingRules.append(commentRule);
}

void ConsoleHighlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : m_highlightingRules) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}