#include "mainwindow.h"
#include "signallayoutwidget.h"
#include "dbcvalidator.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QScreen>
#include <QDebug>
#include <QClipboard>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <vector>

// DBC Motorola 语义：@0 Motorola 时，DBC 的 startBit 表示 MSB 位位置。
// CANoe 期望展示口径：Motorola 的 StartBit 展示为对应的 LSB 位位置。
// 本工程内部仍按 DBC startBit 语义存储，界面显示/导入导出时做转换。
static int motorolaMsbToLsbDisplay(int startBitMsb, int length)
{
    int bitIndex = startBitMsb;
    // 从 MSB 走 length-1 次更新后，得到最后一位（LSB）的物理位索引。
    for (int i = 0; i < length - 1; ++i) {
        if (bitIndex % 8 == 0) {
            bitIndex += 15;
        } else {
            bitIndex -= 1;
        }
    }
    return bitIndex;
}

static int motorolaLsbToMbsInternal(int startBitLsb, int length)
{
    int bitIndex = startBitLsb;
    // 反向从 LSB 走 length-1 次更新，得到内部的 MSB startBit。
    for (int i = 0; i < length - 1; ++i) {
        if (bitIndex % 8 == 7) {
            bitIndex -= 15;
        } else {
            bitIndex += 1;
        }
    }
    return bitIndex;
}

static int displayStartBit(const CanSignal *signal)
{
    if (!signal) {
        return 0;
    }
    const int startBit = signal->getStartBit();
    const int length = signal->getLength();
    if (signal->getByteOrder() == 0) {
        return motorolaMsbToLsbDisplay(startBit, length);
    }
    // Intel：startBit 本身就是 LSB 口径
    return startBit;
}

static QString applyPrefixRule(const QString &name, const QString &newPrefix)
{
    const QString trimmedPrefix = newPrefix.trimmed();
    if (trimmedPrefix.isEmpty()) {
        return name;
    }
    const int idx = name.indexOf(QLatin1Char('_'));
    if (idx > 0) {
        // Replace prefix before first underscore, keep underscore+suffix
        return trimmedPrefix + name.mid(idx);
    }
    // No underscore (or underscore at position 0): directly prepend prefix
    return trimmedPrefix + name;
}

struct PrefixCopyOptions {
    QString prefix;
    bool applyToMessageName = true;
    bool applyToSignalNames = true;
};

static bool askPrefixCopyOptions(QWidget *parent,
                                const QString &title,
                                bool allowMessageOption,
                                PrefixCopyOptions *out)
{
    if (!out) {
        return false;
    }

    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    layout->addLayout(form);

    auto *prefixEdit = new QLineEdit(&dialog);
    prefixEdit->setPlaceholderText(QObject::tr("例如：FR"));
    form->addRow(QObject::tr("新前缀"), prefixEdit);

    auto *applyMsg = new QCheckBox(QObject::tr("修改报文名前缀"), &dialog);
    auto *applySig = new QCheckBox(QObject::tr("修改信号名前缀"), &dialog);
    applyMsg->setChecked(true);
    applySig->setChecked(true);
    applyMsg->setEnabled(allowMessageOption);
    if (!allowMessageOption) {
        applyMsg->setChecked(false);
    }
    layout->addWidget(applyMsg);
    layout->addWidget(applySig);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("确定"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QObject::tr("取消"));
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    out->prefix = prefixEdit->text().trimmed();
    out->applyToMessageName = allowMessageOption && applyMsg->isChecked();
    out->applyToSignalNames = applySig->isChecked();
    return true;
}

static QString uniqueSignalName(const CanMessage *message, const CanSignal *self, const QString &desired)
{
    if (!message) {
        return desired;
    }
    auto exists = [&](const QString &name) -> bool {
        for (CanSignal *s : message->getSignals()) {
            if (!s || s == self) {
                continue;
            }
            if (s->getName() == name) {
                return true;
            }
        }
        return false;
    };
    if (!exists(desired)) {
        return desired;
    }
    int k = 1;
    QString candidate;
    do {
        candidate = desired + QStringLiteral("_") + QString::number(k++);
    } while (exists(candidate));
    return candidate;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dbcParser(new DbcParser())
    , m_currentMessage(nullptr)
    , m_currentSignal(nullptr)
    , m_currentDbcPath()
{
    setupUI();
    setupMenuBar();
    setupStatusBar();
    
    setWindowTitle("DBC Viewer");
    setMinimumSize(1000, 700);
    resize(1200, 800);

    // Center window on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int x = screenGeometry.x() + (screenGeometry.width() - width()) / 2;
        int y = screenGeometry.y() + (screenGeometry.height() - height()) / 2;
        move(x, y);
    }

    // Enable drag and drop
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    delete m_dbcParser;
}

void MainWindow::setupUI()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    
    // Create main layout
    QHBoxLayout *mainLayout = new QHBoxLayout(m_centralWidget);
    
    // Create splitters
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_rightSplitter = new QSplitter(Qt::Vertical, this);
    
    // Left panel - Message tree
    m_messageGroup = new QGroupBox("CAN Messages", this);
    QVBoxLayout *messageLayout = new QVBoxLayout(m_messageGroup);
    
    m_messageTree = new QTreeWidget(this);
    m_messageTree->setHeaderLabels(QStringList() << "ID" << "Name" << "Length" << "Transmitter" << "Cycle Time");
    m_messageTree->setAlternatingRowColors(true);
    m_messageTree->setRootIsDecorated(false);
    m_messageTree->setSortingEnabled(true);
    m_messageTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_messageTree->sortByColumn(0, Qt::AscendingOrder);
    m_messageTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_messageTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_messageTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onMessageSelectionChanged);
    connect(m_messageTree, &QTreeWidget::itemChanged, this, &MainWindow::onMessageTreeItemChanged);
    connect(m_messageTree, &QTreeWidget::customContextMenuRequested, this, &MainWindow::onMessageTreeContextMenuRequested);
    
    messageLayout->addWidget(m_messageTree);
    
    // Right panel - Signal table
    m_signalGroup = new QGroupBox("Signals", this);
    QVBoxLayout *signalLayout = new QVBoxLayout(m_signalGroup);
    
    m_signalTable = new QTableWidget(this);
    m_signalTable->setColumnCount(8);
    m_signalTable->setHorizontalHeaderLabels(QStringList()
        << "Name" << "Start Bit" << "Length" << "Factor" << "Offset"
        << "Min" << "Max" << "Unit");
    m_signalTable->setAlternatingRowColors(true);
    m_signalTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_signalTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_signalTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::SelectedClicked);
    m_signalTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_signalTable->horizontalHeader()->setStretchLastSection(true);
    m_signalTable->horizontalHeader()->setSectionsClickable(true);

    connect(m_signalTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSignalSelectionChanged);
    connect(m_signalTable, &QTableWidget::itemChanged, this, &MainWindow::onSignalCellChanged);
    connect(m_signalTable->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onSignalTableHeaderClicked);
    connect(m_signalTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::onSignalTableContextMenuRequested);
    
    signalLayout->addWidget(m_signalTable);
    
    // Details panel: stack Layout (when CAN ID selected) vs Properties+Value Table (when signal selected)
    m_detailsGroup = new QGroupBox("Signal Details", this);
    QVBoxLayout *detailsLayout = new QVBoxLayout(m_detailsGroup);
    
    m_detailsStack = new QStackedWidget(this);
    m_signalLayout = new SignalLayoutWidget(this);
    m_detailsTabs = new QTabWidget(this);
    
    m_signalDetails = new QTextEdit(this);
    m_signalDetails->setReadOnly(true);
    m_signalDetails->setFont(QFont("Courier", 10));
    
    m_valueTable = new QTextEdit(this);
    m_valueTable->setReadOnly(false);
    m_valueTable->setFont(QFont("Courier", 10));
    
    m_detailsTabs->addTab(m_signalDetails, "Properties");
    m_detailsTabs->addTab(m_valueTable, "Value Table");
    
    m_detailsStack->addWidget(m_signalLayout);   // index 0: message bitfield layout
    m_detailsStack->addWidget(m_detailsTabs);   // index 1: signal properties + value table
    detailsLayout->addWidget(m_detailsStack);

    m_applyValueTableButton = new QPushButton(tr("应用值表变更"), this);
    m_applyValueTableButton->setEnabled(false);
    detailsLayout->addWidget(m_applyValueTableButton);
    connect(m_applyValueTableButton, &QPushButton::clicked, this, [this]() {
        applyValueTableChanges();
    });
    
    // Add to splitters
    m_rightSplitter->addWidget(m_signalGroup);
    m_rightSplitter->addWidget(m_detailsGroup);
    m_rightSplitter->setSizes(QList<int>() << 300 << 200);
    
    m_mainSplitter->addWidget(m_messageGroup);
    m_mainSplitter->addWidget(m_rightSplitter);
    m_mainSplitter->setSizes(QList<int>() << 400 << 600);
    
    mainLayout->addWidget(m_mainSplitter);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    
    QAction *openAction = new QAction("&Open File...", this);
    openAction->setShortcut(QKeySequence::Open);
    openAction->setStatusTip("Open a DBC or Excel file");
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    fileMenu->addAction(openAction);

    QMenu *exportExcelMenu = fileMenu->addMenu("Export to &Excel");
    QAction *exportExcelByEcuAction = new QAction("By &ECU (multiple sheets)...", this);
    exportExcelByEcuAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    exportExcelByEcuAction->setStatusTip("Export with one sheet per ECU (BU_ node)");
    connect(exportExcelByEcuAction, &QAction::triggered, this, &MainWindow::exportToExcelByEcu);
    exportExcelMenu->addAction(exportExcelByEcuAction);
    QAction *exportExcelSingleAction = new QAction("&Single sheet (no ECU split)...", this);
    exportExcelSingleAction->setStatusTip("Export all messages in one data sheet");
    connect(exportExcelSingleAction, &QAction::triggered, this, &MainWindow::exportToExcelSingleSheet);
    exportExcelMenu->addAction(exportExcelSingleAction);

    QAction *exportDbcAction = new QAction("Export to &DBC...", this);
    exportDbcAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    exportDbcAction->setStatusTip("Export the current DBC to a new DBC file");
    connect(exportDbcAction, &QAction::triggered, this, &MainWindow::exportToDbc);
    fileMenu->addAction(exportDbcAction);

    fileMenu->addSeparator();

    QAction *exitAction = new QAction("E&xit", this);
    exitAction->setShortcut(QKeySequence::Quit);
    exitAction->setStatusTip("Exit the application");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    fileMenu->addAction(exitAction);
    
    // Edit menu
    QMenu *editMenu = menuBar->addMenu(tr("&Edit"));
    QAction *saveAction = new QAction(tr("&Save"), this);
    saveAction->setShortcut(QKeySequence::Save);
    QAction *revertAction = new QAction(tr("&Revert to Last Save"), this);
    revertAction->setShortcut(QKeySequence::Undo);
    QAction *addMsgAction = new QAction(tr("Add &Message"), this);
    QAction *delMsgAction = new QAction(tr("Delete Message"), this);
    QAction *addSigAction = new QAction(tr("Add &Signal"), this);
    QAction *delSigAction = new QAction(tr("Delete Signal"), this);

    connect(saveAction, &QAction::triggered, this, [this]() {
        applySaveSnapshot();
        m_statusLabel->setText(tr("已保存当前修改"));
    });
    connect(revertAction, &QAction::triggered, this, [this]() {
        restoreSnapshotToCurrent();
    });
    connect(addMsgAction, &QAction::triggered, this, &MainWindow::addMessage);
    connect(delMsgAction, &QAction::triggered, this, &MainWindow::deleteMessage);
    connect(addSigAction, &QAction::triggered, this, &MainWindow::addSignal);
    connect(delSigAction, &QAction::triggered, this, &MainWindow::deleteSignal);

    editMenu->addAction(saveAction);
    editMenu->addAction(revertAction);
    editMenu->addSeparator();
    editMenu->addAction(addMsgAction);
    editMenu->addAction(delMsgAction);
    editMenu->addSeparator();
    editMenu->addAction(addSigAction);
    editMenu->addAction(delSigAction);

    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    
    QAction *aboutAction = new QAction("&About", this);
    aboutAction->setStatusTip("About DBC Viewer");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    helpMenu->addAction(aboutAction);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready");
    m_fileLabel = new QLabel("No file loaded");
    
    statusBar()->addWidget(m_statusLabel);
    statusBar()->addPermanentWidget(m_fileLabel);
}

void MainWindow::openFile()
{
    QString initialDir;
    if (!m_currentDbcPath.isEmpty()) {
        QFileInfo info(m_currentDbcPath);
        initialDir = info.absolutePath();
    } else {
        initialDir = QDir::homePath();
    }

    const QString fileName = QFileDialog::getOpenFileName(this,
        "Open File", initialDir,
        "DBC and Excel Files (*.dbc *.xlsx);;DBC Files (*.dbc);;Excel Workbook (*.xlsx);;All Files (*)");

    if (fileName.isEmpty()) {
        return;
    }

    if (fileName.endsWith(".xlsx", Qt::CaseInsensitive)) {
        DbcExcelConverter::ImportResult importResult;
        QString errorMessage;
        if (!DbcExcelConverter::importFromExcel(fileName, importResult, &errorMessage)) {
            QMessageBox::critical(this, "Open Failed", errorMessage);
            return;
        }
        m_dbcParser->loadFromExcelImport(importResult);
        m_currentDbcPath = fileName;
        m_fileLabel->setText(QString("File: %1").arg(QFileInfo(fileName).fileName()));
        m_statusLabel->setText(QString("Loaded %1 messages").arg(m_dbcParser->getMessages().size()));
        populateMessageTree();
        clearViews();
        showValidationErrorsIfAny();
        applySaveSnapshot();
    } else {
        loadDbcFile(fileName);
    }
}

void MainWindow::exportToExcelByEcu()
{
    if (m_savedMessages.isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to Excel.");
        return;
    }

    const ValidationResult result = validateMessages(m_savedMessages);
    if (!result.ok) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("导出前校验失败"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("当前数据存在问题，建议先修复再导出。"));
        msgBox.setDetailedText(result.errors.join(QStringLiteral("\n")));
        msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
        msgBox.button(QMessageBox::Ok)->setText(tr("仍然导出"));
        msgBox.button(QMessageBox::Cancel)->setText(tr("取消"));
        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }

    QString suggestedPath;
    if (!m_currentDbcPath.isEmpty()) {
        QFileInfo info(m_currentDbcPath);
        suggestedPath = info.absolutePath() + "/" + info.completeBaseName() + ".xlsx";
    } else {
        suggestedPath = QDir::homePath() + "/dbc_export.xlsx";
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
        "Export to Excel (by ECU)", suggestedPath, "Excel Workbook (*.xlsx)");

    if (filePath.isEmpty()) {
        return;
    }

    QString normalizedPath = filePath;
    if (!normalizedPath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        normalizedPath.append(".xlsx");
    }

    QString errorMessage;
    if (!DbcExcelConverter::exportToExcel(normalizedPath,
                                          m_savedVersion,
                                          m_savedBusType,
                                          m_savedNodes,
                                          m_savedMessages,
                                          m_savedDocumentTitle,
                                          m_savedChangeHistory,
                                          true,
                                          &errorMessage)) {
        QMessageBox::critical(this, "Export Failed", errorMessage);
        return;
    }

    m_statusLabel->setText(QString("Exported Excel (by ECU): %1").arg(QFileInfo(normalizedPath).fileName()));
}

void MainWindow::exportToExcelSingleSheet()
{
    if (m_savedMessages.isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to Excel.");
        return;
    }

    const ValidationResult result = validateMessages(m_savedMessages);
    if (!result.ok) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("导出前校验失败"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("当前数据存在问题，建议先修复再导出。"));
        msgBox.setDetailedText(result.errors.join(QStringLiteral("\n")));
        msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
        msgBox.button(QMessageBox::Ok)->setText(tr("仍然导出"));
        msgBox.button(QMessageBox::Cancel)->setText(tr("取消"));
        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }

    QString suggestedPath;
    if (!m_currentDbcPath.isEmpty()) {
        QFileInfo info(m_currentDbcPath);
        suggestedPath = info.absolutePath() + "/" + info.completeBaseName() + ".xlsx";
    } else {
        suggestedPath = QDir::homePath() + "/dbc_export.xlsx";
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
        "Export to Excel (single sheet)", suggestedPath, "Excel Workbook (*.xlsx)");

    if (filePath.isEmpty()) {
        return;
    }

    QString normalizedPath = filePath;
    if (!normalizedPath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        normalizedPath.append(".xlsx");
    }

    QString errorMessage;
    if (!DbcExcelConverter::exportToExcel(normalizedPath,
                                          m_savedVersion,
                                          m_savedBusType,
                                          m_savedNodes,
                                          m_savedMessages,
                                          m_savedDocumentTitle,
                                          m_savedChangeHistory,
                                          false,
                                          &errorMessage)) {
        QMessageBox::critical(this, "Export Failed", errorMessage);
        return;
    }

    m_statusLabel->setText(QString("Exported Excel (single sheet): %1").arg(QFileInfo(normalizedPath).fileName()));
}

void MainWindow::exportToDbc()
{
    if (m_savedMessages.isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to DBC.");
        return;
    }

    const ValidationResult result = validateMessages(m_savedMessages);
    if (!result.ok) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("导出前校验失败"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("当前数据存在问题，建议先修复再导出。"));
        msgBox.setDetailedText(result.errors.join(QStringLiteral("\n")));
        msgBox.setStandardButtons(QMessageBox::Cancel | QMessageBox::Ok);
        msgBox.button(QMessageBox::Ok)->setText(tr("仍然导出"));
        msgBox.button(QMessageBox::Cancel)->setText(tr("取消"));
        if (msgBox.exec() == QMessageBox::Cancel) {
            return;
        }
    }

    QString suggestedPath;
    if (!m_currentDbcPath.isEmpty()) {
        QFileInfo info(m_currentDbcPath);
        suggestedPath = info.absolutePath() + "/" + info.completeBaseName() + ".dbc";
    } else {
        suggestedPath = QDir::homePath() + "/dbc.dbc";
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
        "Export to DBC", suggestedPath, "DBC Files (*.dbc);;All Files (*)");

    if (filePath.isEmpty()) {
        return;
    }

    QString normalizedPath = filePath;
    if (!normalizedPath.endsWith(".dbc", Qt::CaseInsensitive)) {
        normalizedPath.append(".dbc");
    }

    QString errorMessage;
    if (!DbcWriter::write(normalizedPath,
                          m_savedVersion,
                          m_savedBusType,
                          m_savedNodes,
                          m_savedMessages,
                          QString(),
                          m_savedDocumentTitle,
                          m_savedChangeHistory,
                          m_savedGlobalValueTables,
                          &errorMessage)) {
        QMessageBox::critical(this, "Export Failed", errorMessage);
        return;
    }

    m_statusLabel->setText(QString("Exported DBC: %1").arg(QFileInfo(normalizedPath).fileName()));
}

void MainWindow::loadDbcFile(const QString &filePath)
{
    if (m_dbcParser->parseFile(filePath)) {
        m_currentDbcPath = filePath;
        m_fileLabel->setText(QString("File: %1").arg(QFileInfo(filePath).fileName()));
        m_statusLabel->setText(QString("Loaded %1 messages").arg(m_dbcParser->getMessages().size()));

        populateMessageTree();
        clearViews();
        showValidationErrorsIfAny();
        applySaveSnapshot();
    } else {
        QMessageBox::critical(this, "Error", "Failed to parse DBC file!");
        m_statusLabel->setText("Error loading file");
    }
}

void MainWindow::populateMessageTree()
{
    m_isUpdatingMessageTree = true;
    m_messageTree->clear();
    
    for (CanMessage *message : m_dbcParser->getMessages()) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_messageTree);
        item->setText(0, message->getFormattedId());
        item->setText(1, message->getName());
        item->setText(2, QString::number(message->getLength()));
        item->setText(3, message->getTransmitter());
        const QString cycleText = message->getCycleTime() > 0
            ? QString("%1 ms (%2)").arg(message->getCycleTime()).arg(message->getSendType().isEmpty() ? "N/A" : message->getSendType())
            : (message->getSendType().isEmpty() ? "N/A" : message->getSendType());
        item->setText(4, cycleText);
        
        // Store message pointer in item data
        item->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(message)));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        
        // Add signals as child items
        for (CanSignal *signal : message->getSignals()) {
            QTreeWidgetItem *signalItem = new QTreeWidgetItem(item);
            signalItem->setText(0, signal->getName());
            signalItem->setText(1, QString("Bit %1").arg(displayStartBit(signal)));
            signalItem->setText(2, QString("%1 bits").arg(signal->getLength()));
            signalItem->setText(3, signal->getUnit());
            signalItem->setText(4, signal->getReceiversAsString());
            
            // Store signal pointer in item data
            signalItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(signal)));
            signalItem->setFlags(signalItem->flags() & ~Qt::ItemIsEditable);
        }
    }
    
    m_messageTree->collapseAll();
    m_isUpdatingMessageTree = false;
}

void MainWindow::populateSignalTable(CanMessage *message)
{
    if (!message) {
        m_isUpdatingSignalTable = true;
        m_signalTable->setRowCount(0);
        m_isUpdatingSignalTable = false;
        return;
    }
    
    QList<CanSignal*> signalList = message->getSignals();
    // Default ordering: sort by StartBit (UI/CANoe口径)
    std::sort(signalList.begin(), signalList.end(), [](const CanSignal *a, const CanSignal *b) {
        const int sa = displayStartBit(a);
        const int sb = displayStartBit(b);
        if (sa != sb) {
            return sa < sb;
        }
        const QString na = a ? a->getName() : QString();
        const QString nb = b ? b->getName() : QString();
        return na < nb;
    });
    m_signalTable->setRowCount(signalList.size());
    
    for (int i = 0; i < signalList.size(); ++i) {
        CanSignal *signal = signalList[i];
        
        m_isUpdatingSignalTable = true;

        auto *nameItem = new QTableWidgetItem(signal->getName());
        auto *startBitItem = new QTableWidgetItem(QString::number(displayStartBit(signal)));
        auto *lengthItem = new QTableWidgetItem(QString::number(signal->getLength()));
        auto *factorItem = new QTableWidgetItem(QString::number(signal->getFactor()));
        auto *offsetItem = new QTableWidgetItem(QString::number(signal->getOffset()));
        auto *minItem = new QTableWidgetItem(QString::number(signal->getMin()));
        auto *maxItem = new QTableWidgetItem(QString::number(signal->getMax()));
        auto *unitItem = new QTableWidgetItem(signal->getUnit());

        m_signalTable->setItem(i, 0, nameItem);
        m_signalTable->setItem(i, 1, startBitItem);
        m_signalTable->setItem(i, 2, lengthItem);
        m_signalTable->setItem(i, 3, factorItem);
        m_signalTable->setItem(i, 4, offsetItem);
        m_signalTable->setItem(i, 5, minItem);
        m_signalTable->setItem(i, 6, maxItem);
        m_signalTable->setItem(i, 7, unitItem);

        // Store model pointer and original numeric values for validation/restore
        nameItem->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(signal)));
        m_signalTable->item(i, 1)->setData(Qt::UserRole, displayStartBit(signal));
        m_signalTable->item(i, 2)->setData(Qt::UserRole, signal->getLength());
        m_signalTable->item(i, 3)->setData(Qt::UserRole, signal->getFactor());
        m_signalTable->item(i, 4)->setData(Qt::UserRole, signal->getOffset());
        m_signalTable->item(i, 5)->setData(Qt::UserRole, signal->getMin());
        m_signalTable->item(i, 6)->setData(Qt::UserRole, signal->getMax());
        m_isUpdatingSignalTable = false;
    }
    
    m_signalTable->resizeColumnsToContents();
}

void MainWindow::onSignalCellChanged(QTableWidgetItem *item)
{
    if (!item || m_isUpdatingSignalTable) {
        return;
    }

    const int row = item->row();
    const int col = item->column();
    if (row < 0 || col < 0) {
        return;
    }

    QTableWidgetItem *nameItem = m_signalTable->item(row, 0);
    if (!nameItem) {
        return;
    }

    void *ptr = nameItem->data(Qt::UserRole).value<void *>();
    CanSignal *signal = static_cast<CanSignal *>(ptr);
    if (!signal) {
        return;
    }

    const QString text = item->text().trimmed();

    m_isUpdatingSignalTable = true;

    bool ok = false;
    switch (col) {
    case 0: // Name
        signal->setName(text);
        markDirty();
        break;
    case 1: { // Start Bit
        int value = text.toInt(&ok);
        if (!ok) {
            item->setText(QString::number(displayStartBit(signal)));
            m_statusLabel->setText(tr("无效的起始位：%1").arg(text));
        } else {
            // UI 展示的是 CANoe 口径：Motorola 的 StartBit 显示为 LSB
            const int internalStartBit = (signal->getByteOrder() == 0)
                ? motorolaLsbToMbsInternal(value, signal->getLength())
                : value;
            signal->setStartBit(internalStartBit);
            item->setData(Qt::UserRole, value); // store display value
            markDirty();
        }
        break;
    }
    case 2: { // Length
        int value = text.toInt(&ok);
        if (!ok || value <= 0) {
            item->setText(QString::number(signal->getLength()));
            m_statusLabel->setText(tr("无效的长度：%1").arg(text));
        } else {
            signal->setLength(value);
            item->setData(Qt::UserRole, value);
            // Motorola 的展示 StartBit（LSB）会随长度变化，这里同步更新表格里的 Start Bit 单元格。
            const int newDisplayStartBit = displayStartBit(signal);
            if (QTableWidgetItem *startBitItem = m_signalTable->item(row, 1)) {
                startBitItem->setText(QString::number(newDisplayStartBit));
                startBitItem->setData(Qt::UserRole, newDisplayStartBit);
            }
            markDirty();
        }
        break;
    }
    case 3: { // Factor
        double value = text.toDouble(&ok);
        if (!ok) {
            item->setText(QString::number(signal->getFactor()));
            m_statusLabel->setText(tr("无效的系数：%1").arg(text));
        } else {
            signal->setFactor(value);
            item->setData(Qt::UserRole, value);
            markDirty();
        }
        break;
    }
    case 4: { // Offset
        double value = text.toDouble(&ok);
        if (!ok) {
            item->setText(QString::number(signal->getOffset()));
            m_statusLabel->setText(tr("无效的偏移：%1").arg(text));
        } else {
            signal->setOffset(value);
            item->setData(Qt::UserRole, value);
            markDirty();
        }
        break;
    }
    case 5: { // Min
        double value = text.toDouble(&ok);
        if (!ok) {
            item->setText(QString::number(signal->getMin()));
            m_statusLabel->setText(tr("无效的最小值：%1").arg(text));
        } else {
            signal->setMin(value);
            item->setData(Qt::UserRole, value);
            markDirty();
        }
        break;
    }
    case 6: { // Max
        double value = text.toDouble(&ok);
        if (!ok) {
            item->setText(QString::number(signal->getMax()));
            m_statusLabel->setText(tr("无效的最大值：%1").arg(text));
        } else {
            signal->setMax(value);
            item->setData(Qt::UserRole, value);
            markDirty();
        }
        break;
    }
    case 7: // Unit
        signal->setUnit(text);
        markDirty();
        break;
    default:
        break;
    }

    // 刷新位布局和详情
    if (m_currentMessage) {
        m_signalLayout->setMessage(m_currentMessage);
    }
    if (m_currentSignal == signal) {
        populateSignalDetails(m_currentSignal);
    }

    m_isUpdatingSignalTable = false;
}

void MainWindow::onSignalTableHeaderClicked(int logicalIndex)
{
    if (logicalIndex < 0 || logicalIndex >= m_signalTable->columnCount()) {
        return;
    }
    if (m_signalTable->rowCount() <= 1) {
        return;
    }
    if (m_signalTableSortColumn == logicalIndex) {
        m_signalTableSortOrder = (m_signalTableSortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_signalTableSortColumn = logicalIndex;
        m_signalTableSortOrder = Qt::AscendingOrder;
    }

    const int rowCount = m_signalTable->rowCount();
    const int colCount = m_signalTable->columnCount();
    QList<QList<QTableWidgetItem *>> rows;
    for (int r = 0; r < rowCount; ++r) {
        QList<QTableWidgetItem *> row;
        for (int c = 0; c < colCount; ++c) {
            row.append(m_signalTable->takeItem(r, c));
        }
        rows.append(row);
    }

    const bool numericColumn = (logicalIndex >= 1 && logicalIndex <= 6);
    const bool ascending = (m_signalTableSortOrder == Qt::AscendingOrder);
    if (numericColumn) {
        std::sort(rows.begin(), rows.end(), [logicalIndex, ascending](const QList<QTableWidgetItem *> &a, const QList<QTableWidgetItem *> &b) {
            const QVariant va = a[logicalIndex]->data(Qt::UserRole);
            const QVariant vb = b[logicalIndex]->data(Qt::UserRole);
            bool okA = false, okB = false;
            const double na = va.toDouble(&okA);
            const double nb = vb.toDouble(&okB);
            if (okA && okB) {
                return ascending ? (na < nb) : (na > nb);
            }
            const QString sa = a[logicalIndex]->text();
            const QString sb = b[logicalIndex]->text();
            return ascending ? (sa < sb) : (sa > sb);
        });
    } else {
        std::sort(rows.begin(), rows.end(), [logicalIndex, ascending](const QList<QTableWidgetItem *> &a, const QList<QTableWidgetItem *> &b) {
            const QString sa = a[logicalIndex]->text();
            const QString sb = b[logicalIndex]->text();
            return ascending ? (sa < sb) : (sa > sb);
        });
    }

    for (int r = 0; r < rowCount; ++r) {
        for (int c = 0; c < colCount; ++c) {
            m_signalTable->setItem(r, c, rows[r][c]);
        }
    }
}

void MainWindow::populateSignalDetails(CanSignal *signal)
{
    if (!signal) {
        m_signalDetails->clear();
        m_valueTable->clear();
        m_applyValueTableButton->setEnabled(false);
        return;
    }
    
    // Signal properties
    auto maskForLength = [](int length) -> quint64 {
        if (length <= 0) {
            return 0;
        }
        if (length >= 64) {
            return std::numeric_limits<quint64>::max();
        }
        return (quint64(1) << length) - 1;
    };
    const quint64 initialValue = static_cast<quint64>(std::llround(signal->getInitialValue())) & maskForLength(signal->getLength());
    // DBC 约定：@0 = Motorola（startBit 为 MSB），@1 = Intel（startBit 为 LSB）
    const QString byteOrderText = signal->getByteOrder() == 0 ? "Motorola MSB" : "Intel LSB";
    const QString receivers = signal->getReceiversAsString().isEmpty() ? "N/A" : signal->getReceiversAsString();
    const QString sendType = signal->getSendType().isEmpty() ? "N/A" : signal->getSendType();
    const QString invalidValue = signal->getInvalidValueHex().isEmpty() ? "-" : signal->getInvalidValueHex();
    const QString inactiveValue = signal->getInactiveValueHex().isEmpty() ? "-" : signal->getInactiveValueHex();

    QString details = QString(
        "Name: %1\n"
        "Start Bit: %2\n"
        "Length: %3 bits\n"
        "Byte Order: %4\n"
        "Signed: %5\n"
        "Send Type: %6\n"
        "Factor: %7\n"
        "Offset: %8\n"
        "Min Value: %9\n"
        "Max Value: %10\n"
        "Unit: %11\n"
        "Receivers: %12\n"
        "Initial Value (Hex): %13\n"
        "Invalid Value (Hex): %14\n"
        "Inactive Value (Hex): %15\n"
        "\nPhysical Value Calculation:\n"
        "Physical = Raw × %7 + %8\n"
        "Raw = (Physical - %8) ÷ %7"
    ).arg(signal->getName())
     .arg(displayStartBit(signal))
     .arg(signal->getLength())
     .arg(byteOrderText)
     .arg(signal->isSigned() ? "Yes" : "No")
     .arg(sendType)
     .arg(signal->getFactor())
     .arg(signal->getOffset())
     .arg(signal->getMin())
     .arg(signal->getMax())
     .arg(signal->getUnit())
     .arg(receivers)
     .arg(QString("0x%1").arg(initialValue, 0, 16).toUpper())
     .arg(invalidValue)
     .arg(inactiveValue);
    
    m_signalDetails->setPlainText(details);
    
    // Value table
    QMap<int, QString> valueTable = signal->getValueTable();
    if (valueTable.isEmpty()) {
        m_valueTable->setPlainText("No value table defined for this signal.");
    } else {
        QString valueTableText;
        for (auto it = valueTable.begin(); it != valueTable.end(); ++it) {
            valueTableText += QString("%1: %2\n").arg(it.key()).arg(it.value());
        }
        m_valueTable->setPlainText(valueTableText);
    }
    m_applyValueTableButton->setEnabled(true);
}

void MainWindow::applyValueTableChanges()
{
    if (!m_currentSignal) {
        return;
    }

    const QString text = m_valueTable->toPlainText();
    const QStringList lines = text.split(QLatin1Char('\n'));
    QMap<int, QString> table;

    int lineNumber = 0;
    for (const QString &line : lines) {
        ++lineNumber;
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        const int colonPos = trimmed.indexOf(QLatin1Char(':'));
        if (colonPos <= 0) {
            QMessageBox::warning(this, tr("值表解析错误"),
                                 tr("第 %1 行缺少冒号（格式应为 \"原始值: 描述\"）。").arg(lineNumber));
            return;
        }
        const QString rawStr = trimmed.left(colonPos).trimmed();
        const QString desc = trimmed.mid(colonPos + 1).trimmed();
        bool ok = false;
        const int raw = rawStr.toInt(&ok);
        if (!ok) {
            QMessageBox::warning(this, tr("值表解析错误"),
                                 tr("第 %1 行的原始值 \"%2\" 不是有效整数。").arg(lineNumber).arg(rawStr));
            return;
        }
        table[raw] = desc;
    }

    m_currentSignal->setValueTable(table);
    populateSignalDetails(m_currentSignal);
    m_statusLabel->setText(tr("值表已更新"));
    markDirty();
}

void MainWindow::onMessageSelectionChanged()
{
    QList<QTreeWidgetItem*> selectedItems = m_messageTree->selectedItems();
    
    if (selectedItems.isEmpty()) {
        m_currentMessage = nullptr;
        m_currentSignal = nullptr;
        populateSignalTable(nullptr);
        m_signalLayout->setMessage(nullptr);
        m_signalLayout->setHighlightedSignal(nullptr);
        m_detailsStack->setCurrentIndex(0);
        m_signalDetails->clear();
        m_valueTable->clear();
        if (m_applyValueTableButton) {
            m_applyValueTableButton->setEnabled(false);
        }
        return;
    }
    
    QTreeWidgetItem *item = selectedItems.first();
    void *data = item->data(0, Qt::UserRole).value<void*>();
    
    if (item->parent() == nullptr) {
        // CAN ID (message) selected -> show bitfield layout
        m_currentMessage = static_cast<CanMessage*>(data);
        m_currentSignal = nullptr;
        populateSignalTable(m_currentMessage);
        m_signalLayout->setMessage(m_currentMessage);
        m_signalLayout->setHighlightedSignal(nullptr);
        m_detailsStack->setCurrentIndex(0);
    } else {
        // Specific signal selected in tree -> show Properties + Value Table
        m_currentSignal = static_cast<CanSignal*>(data);
        m_currentMessage = static_cast<CanMessage*>(item->parent()->data(0, Qt::UserRole).value<void*>());
        populateSignalTable(m_currentMessage);
        m_signalLayout->setMessage(m_currentMessage);
        m_signalLayout->setHighlightedSignal(m_currentSignal);
        m_detailsStack->setCurrentIndex(1);
        populateSignalDetails(m_currentSignal);
    }
}

void MainWindow::onMessageTreeContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = m_messageTree->itemAt(pos);
    if (!item) {
        return;
    }
    // If right-clicked item is not part of the current selection, switch selection to it.
    if (!item->isSelected()) {
        m_messageTree->clearSelection();
        m_messageTree->setCurrentItem(item);
        item->setSelected(true);
    }
    QMenu menu(this);
    QAction *copyRow = menu.addAction(tr("复制为新的报文"));
    QAction *copyPrefix = menu.addAction(tr("复制并修改前缀..."));
    QAction *renamePrefix = menu.addAction(tr("修改前缀..."));
    QAction *deleteRow = menu.addAction(tr("删除报文"));
    QAction *chosen = menu.exec(m_messageTree->viewport()->mapToGlobal(pos));
    if (chosen == copyRow) {
        copyMessageAsNew();
    } else if (chosen == copyPrefix) {
        copyMessageWithPrefix();
    } else if (chosen == renamePrefix) {
        renameMessagePrefix();
    } else if (chosen == deleteRow) {
        deleteMessage();
    }
}

void MainWindow::onSignalTableContextMenuRequested(const QPoint &pos)
{
    if (!m_signalTable->itemAt(pos)) {
        return;
    }
    int row = m_signalTable->itemAt(pos)->row();
    QMenu menu(this);
    QAction *copyRow = menu.addAction(tr("复制为新的信号"));
    QAction *copyPrefix = menu.addAction(tr("复制并修改前缀..."));
    QAction *renamePrefix = menu.addAction(tr("修改前缀..."));
    QAction *deleteRow = menu.addAction(tr("删除信号"));
    // If right-clicked row is not selected, switch selection to it.
    if (!m_signalTable->item(row, 0)->isSelected()) {
        m_signalTable->clearSelection();
        m_signalTable->selectRow(row);
    }
    QAction *chosen = menu.exec(m_signalTable->viewport()->mapToGlobal(pos));
    if (chosen == copyRow) {
        copySignalAsNew(row);
    } else if (chosen == copyPrefix) {
        copySignalWithPrefix(row);
    } else if (chosen == renamePrefix) {
        renameSignalPrefix(row);
    } else if (chosen == deleteRow) {
        deleteSignalAtRow(row);
    }
}

void MainWindow::copyMessageAsNew()
{
    if (!m_dbcParser) {
        return;
    }
    const QList<QTreeWidgetItem *> items = m_messageTree->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    // 收集所有被选中的报文（若选中的是信号行，则映射到父报文）
    std::set<CanMessage *> selectedMessages;
    for (QTreeWidgetItem *it : items) {
        if (!it) {
            continue;
        }
        QTreeWidgetItem *msgItem = it->parent() != nullptr ? it->parent() : it;
        void *ptr = msgItem->data(0, Qt::UserRole).value<void *>();
        CanMessage *origMsg = static_cast<CanMessage *>(ptr);
        if (origMsg) {
            selectedMessages.insert(origMsg);
        }
    }
    if (selectedMessages.empty()) {
        return;
    }

    // 计算新的唯一 ID
    quint32 maxId = 0;
    for (CanMessage *msg : m_dbcParser->getMessages()) {
        if (msg && msg->getId() > maxId) {
            maxId = msg->getId();
        }
    }

    // 拷贝报文（批量）
    quint32 nextId = maxId + 1;
    int copyIndex = 1;
    for (CanMessage *origMsg : selectedMessages) {
        if (!origMsg) {
            continue;
        }
        CanMessage *msg = new CanMessage();
        msg->setId(nextId++);

        QString baseName = origMsg->getName();
        if (baseName.isEmpty()) {
            baseName = QStringLiteral("Message");
        }
        const QString suffix = selectedMessages.size() > 1
            ? QStringLiteral("_Copy") + QString::number(copyIndex++)
            : QStringLiteral("_Copy");
        msg->setName(baseName + suffix);
        msg->setLength(origMsg->getLength());
        msg->setTransmitter(origMsg->getTransmitter());
        msg->setCycleTime(origMsg->getCycleTime());
        msg->setFrameFormat(origMsg->getFrameFormat());
        msg->setSendType(origMsg->getSendType());
        msg->setCycleTimeFast(origMsg->getCycleTimeFast());
        msg->setNrOfRepetitions(origMsg->getNrOfRepetitions());
        msg->setDelayTime(origMsg->getDelayTime());
        msg->setComment(origMsg->getComment());
        msg->setMessageType(origMsg->getMessageType());
        msg->setReceivers(origMsg->getReceivers());

        for (CanSignal *origSig : origMsg->getSignals()) {
            if (!origSig) {
                continue;
            }
            CanSignal *sig = new CanSignal();
            sig->setName(origSig->getName());
            sig->setStartBit(origSig->getStartBit());
            sig->setLength(origSig->getLength());
            sig->setByteOrder(origSig->getByteOrder());
            sig->setSigned(origSig->isSigned());
            sig->setFactor(origSig->getFactor());
            sig->setOffset(origSig->getOffset());
            sig->setMin(origSig->getMin());
            sig->setMax(origSig->getMax());
            sig->setUnit(origSig->getUnit());
            sig->setReceivers(origSig->getReceivers());
            sig->setDescription(origSig->getDescription());
            sig->setSendType(origSig->getSendType());
            sig->setInitialValue(origSig->getInitialValue());
            sig->setInvalidValueHex(origSig->getInvalidValueHex());
            sig->setInactiveValueHex(origSig->getInactiveValueHex());
            sig->setValueTable(origSig->getValueTable());
            if (origSig->hasRawRange()) {
                sig->setRawRange(origSig->getRawMin(), origSig->getRawMax());
            }
            msg->addSignal(sig);
        }

        m_dbcParser->addMessage(msg);
    }

    populateMessageTree();
    markDirty();
}

void MainWindow::copyMessageWithPrefix()
{
    if (!m_dbcParser) {
        return;
    }

    PrefixCopyOptions opt;
    if (!askPrefixCopyOptions(this, tr("复制并修改前缀（报文/信号）"), true, &opt)) {
        return;
    }
    if (!opt.applyToMessageName && !opt.applyToSignalNames) {
        return;
    }

    const QList<QTreeWidgetItem *> items = m_messageTree->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    std::set<CanMessage *> selectedMessages;
    for (QTreeWidgetItem *it : items) {
        if (!it) {
            continue;
        }
        QTreeWidgetItem *msgItem = it->parent() != nullptr ? it->parent() : it;
        void *ptr = msgItem->data(0, Qt::UserRole).value<void *>();
        CanMessage *origMsg = static_cast<CanMessage *>(ptr);
        if (origMsg) {
            selectedMessages.insert(origMsg);
        }
    }
    if (selectedMessages.empty()) {
        return;
    }

    // Unique ID
    quint32 maxId = 0;
    for (CanMessage *msg : m_dbcParser->getMessages()) {
        if (msg && msg->getId() > maxId) {
            maxId = msg->getId();
        }
    }

    // Existing message names for collision avoidance
    std::set<QString> existingMsgNames;
    for (CanMessage *msg : m_dbcParser->getMessages()) {
        if (msg) {
            existingMsgNames.insert(msg->getName());
        }
    }

    quint32 nextId = maxId + 1;
    for (CanMessage *origMsg : selectedMessages) {
        if (!origMsg) {
            continue;
        }

        CanMessage *msg = new CanMessage();
        msg->setId(nextId++);

        const QString baseMsgName = origMsg->getName().isEmpty() ? QStringLiteral("Message") : origMsg->getName();
        QString newMsgName = opt.applyToMessageName ? applyPrefixRule(baseMsgName, opt.prefix) : baseMsgName;
        // Avoid duplicate message names
        if (existingMsgNames.count(newMsgName)) {
            const QString base = newMsgName + QStringLiteral("_Copy");
            QString candidate = base;
            int k = 1;
            while (existingMsgNames.count(candidate)) {
                candidate = base + QString::number(k++);
            }
            newMsgName = candidate;
        }
        existingMsgNames.insert(newMsgName);
        msg->setName(newMsgName);

        msg->setLength(origMsg->getLength());
        msg->setTransmitter(origMsg->getTransmitter());
        msg->setCycleTime(origMsg->getCycleTime());
        msg->setFrameFormat(origMsg->getFrameFormat());
        msg->setSendType(origMsg->getSendType());
        msg->setCycleTimeFast(origMsg->getCycleTimeFast());
        msg->setNrOfRepetitions(origMsg->getNrOfRepetitions());
        msg->setDelayTime(origMsg->getDelayTime());
        msg->setComment(origMsg->getComment());
        msg->setMessageType(origMsg->getMessageType());
        msg->setReceivers(origMsg->getReceivers());

        // Track signal name collisions within new message
        std::set<QString> newSigNames;
        for (CanSignal *origSig : origMsg->getSignals()) {
            if (!origSig) {
                continue;
            }
            CanSignal *sig = new CanSignal();
            const QString baseSigName = origSig->getName().isEmpty() ? QStringLiteral("Signal") : origSig->getName();
            QString newSigName = opt.applyToSignalNames ? applyPrefixRule(baseSigName, opt.prefix) : baseSigName;
            // Avoid duplicates within the copied message
            if (newSigNames.count(newSigName)) {
                const QString base = newSigName + QStringLiteral("_Copy");
                QString candidate = base;
                int k = 1;
                while (newSigNames.count(candidate)) {
                    candidate = base + QString::number(k++);
                }
                newSigName = candidate;
            }
            newSigNames.insert(newSigName);

            sig->setName(newSigName);
            sig->setStartBit(origSig->getStartBit());
            sig->setLength(origSig->getLength());
            sig->setByteOrder(origSig->getByteOrder());
            sig->setSigned(origSig->isSigned());
            sig->setFactor(origSig->getFactor());
            sig->setOffset(origSig->getOffset());
            sig->setMin(origSig->getMin());
            sig->setMax(origSig->getMax());
            sig->setUnit(origSig->getUnit());
            sig->setReceivers(origSig->getReceivers());
            sig->setDescription(origSig->getDescription());
            sig->setSendType(origSig->getSendType());
            sig->setInitialValue(origSig->getInitialValue());
            sig->setInvalidValueHex(origSig->getInvalidValueHex());
            sig->setInactiveValueHex(origSig->getInactiveValueHex());
            sig->setValueTable(origSig->getValueTable());
            if (origSig->hasRawRange()) {
                sig->setRawRange(origSig->getRawMin(), origSig->getRawMax());
            }
            msg->addSignal(sig);
        }

        m_dbcParser->addMessage(msg);
    }

    populateMessageTree();
    markDirty();
}

void MainWindow::renameMessagePrefix()
{
    if (!m_dbcParser) {
        return;
    }

    PrefixCopyOptions opt;
    if (!askPrefixCopyOptions(this, tr("修改前缀（报文/信号）"), true, &opt)) {
        return;
    }
    if (!opt.applyToMessageName && !opt.applyToSignalNames) {
        return;
    }

    const QList<QTreeWidgetItem *> items = m_messageTree->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    std::set<CanMessage *> selectedMessages;
    for (QTreeWidgetItem *it : items) {
        if (!it) continue;
        QTreeWidgetItem *msgItem = it->parent() != nullptr ? it->parent() : it;
        void *ptr = msgItem->data(0, Qt::UserRole).value<void *>();
        CanMessage *msg = static_cast<CanMessage *>(ptr);
        if (msg) {
            selectedMessages.insert(msg);
        }
    }
    if (selectedMessages.empty()) {
        return;
    }

    // Existing message names (excluding ones being edited) for collision avoidance
    std::set<QString> otherMsgNames;
    for (CanMessage *m : m_dbcParser->getMessages()) {
        if (!m) continue;
        if (selectedMessages.count(m)) continue;
        otherMsgNames.insert(m->getName());
    }

    for (CanMessage *msg : selectedMessages) {
        if (!msg) continue;

        if (opt.applyToMessageName) {
            const QString baseMsgName = msg->getName().isEmpty() ? QStringLiteral("Message") : msg->getName();
            QString newMsgName = applyPrefixRule(baseMsgName, opt.prefix);
            if (otherMsgNames.count(newMsgName)) {
                // disambiguate
                const QString base = newMsgName;
                int k = 1;
                QString candidate = base;
                while (otherMsgNames.count(candidate)) {
                    candidate = base + QStringLiteral("_") + QString::number(k++);
                }
                newMsgName = candidate;
            }
            otherMsgNames.insert(newMsgName);
            msg->setName(newMsgName);
        }

        if (opt.applyToSignalNames) {
            for (CanSignal *sig : msg->getSignals()) {
                if (!sig) continue;
                const QString baseSigName = sig->getName().isEmpty() ? QStringLiteral("Signal") : sig->getName();
                const QString desired = applyPrefixRule(baseSigName, opt.prefix);
                sig->setName(uniqueSignalName(msg, sig, desired));
            }
        }
    }

    populateMessageTree();
    if (m_currentMessage) {
        populateSignalTable(m_currentMessage);
        m_signalLayout->setMessage(m_currentMessage);
    }
    if (m_currentSignal) {
        populateSignalDetails(m_currentSignal);
    }
    markDirty();
}

void MainWindow::copySignalAsNew(int row)
{
    if (!m_currentMessage) {
        return;
    }
    // Shift 多选后：如果 row 在选中行集合里，则复制所有选中信号；否则只复制该 row。
    std::set<int> selectedRows;
    for (QTableWidgetItem *it : m_signalTable->selectedItems()) {
        if (it) {
            selectedRows.insert(it->row());
        }
    }
    if (selectedRows.empty()) {
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    } else if (selectedRows.find(row) == selectedRows.end()) {
        selectedRows.clear();
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    }

    // 先收集原始信号指针，避免复制过程中刷新导致行索引变化
    std::vector<CanSignal *> origSignals;
    std::set<CanSignal *> origSet;
    for (int r : selectedRows) {
        if (r < 0 || r >= m_signalTable->rowCount()) {
            continue;
        }
        QTableWidgetItem *nameItem = m_signalTable->item(r, 0);
        if (!nameItem) {
            continue;
        }
        void *ptr = nameItem->data(Qt::UserRole).value<void *>();
        CanSignal *origSig = static_cast<CanSignal *>(ptr);
        if (!origSig) {
            continue;
        }
        if (origSet.insert(origSig).second) {
            origSignals.push_back(origSig);
        }
    }
    if (origSignals.empty()) {
        return;
    }

    int copyIndex = 1;
    for (CanSignal *origSig : origSignals) {
        if (!origSig) {
            continue;
        }
        CanSignal *sig = new CanSignal();

        QString baseName = origSig->getName();
        if (baseName.isEmpty()) {
            baseName = QStringLiteral("Signal");
        }

        QString candidate = baseName + QStringLiteral("_Copy");
        if (origSignals.size() > 1) {
            candidate += QString::number(copyIndex++);
        }
        // 避免重名（同一报文内）
        QString finalName = candidate;
        int nameTry = 1;
        while (m_currentMessage->getSignal(finalName) != nullptr) {
            finalName = candidate + QStringLiteral("_") + QString::number(nameTry++);
        }
        sig->setName(finalName);

        sig->setStartBit(origSig->getStartBit());
        sig->setLength(origSig->getLength());
        sig->setByteOrder(origSig->getByteOrder());
        sig->setSigned(origSig->isSigned());
        sig->setFactor(origSig->getFactor());
        sig->setOffset(origSig->getOffset());
        sig->setMin(origSig->getMin());
        sig->setMax(origSig->getMax());
        sig->setUnit(origSig->getUnit());
        sig->setReceivers(origSig->getReceivers());
        sig->setDescription(origSig->getDescription());
        sig->setSendType(origSig->getSendType());
        sig->setInitialValue(origSig->getInitialValue());
        sig->setInvalidValueHex(origSig->getInvalidValueHex());
        sig->setInactiveValueHex(origSig->getInactiveValueHex());
        sig->setValueTable(origSig->getValueTable());
        if (origSig->hasRawRange()) {
            sig->setRawRange(origSig->getRawMin(), origSig->getRawMax());
        }

        m_currentMessage->addSignal(sig);
    }

    populateMessageTree();
    populateSignalTable(m_currentMessage);
    m_signalLayout->setMessage(m_currentMessage);
    markDirty();
}

void MainWindow::copySignalWithPrefix(int row)
{
    if (!m_currentMessage) {
        return;
    }

    PrefixCopyOptions opt;
    if (!askPrefixCopyOptions(this, tr("复制并修改前缀（信号）"), false, &opt)) {
        return;
    }
    if (!opt.applyToSignalNames) {
        return;
    }

    // 与 copySignalAsNew 一致：若 row 在选中集合里，复制选中集合；否则只复制 row
    std::set<int> selectedRows;
    for (QTableWidgetItem *it : m_signalTable->selectedItems()) {
        if (it) {
            selectedRows.insert(it->row());
        }
    }
    if (selectedRows.empty()) {
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    } else if (selectedRows.find(row) == selectedRows.end()) {
        selectedRows.clear();
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    }

    std::vector<CanSignal *> origSignals;
    std::set<CanSignal *> origSet;
    for (int r : selectedRows) {
        if (r < 0 || r >= m_signalTable->rowCount()) {
            continue;
        }
        QTableWidgetItem *nameItem = m_signalTable->item(r, 0);
        if (!nameItem) {
            continue;
        }
        void *ptr = nameItem->data(Qt::UserRole).value<void *>();
        CanSignal *origSig = static_cast<CanSignal *>(ptr);
        if (!origSig) {
            continue;
        }
        if (origSet.insert(origSig).second) {
            origSignals.push_back(origSig);
        }
    }
    if (origSignals.empty()) {
        return;
    }

    int copyIndex = 1;
    for (CanSignal *origSig : origSignals) {
        if (!origSig) {
            continue;
        }
        CanSignal *sig = new CanSignal();

        const QString baseName = origSig->getName().isEmpty() ? QStringLiteral("Signal") : origSig->getName();
        QString candidate = applyPrefixRule(baseName, opt.prefix);
        if (origSignals.size() > 1) {
            candidate += QString::number(copyIndex++);
        }
        QString finalName = candidate;
        int nameTry = 1;
        while (m_currentMessage->getSignal(finalName) != nullptr) {
            finalName = candidate + QStringLiteral("_") + QString::number(nameTry++);
        }
        sig->setName(finalName);

        sig->setStartBit(origSig->getStartBit());
        sig->setLength(origSig->getLength());
        sig->setByteOrder(origSig->getByteOrder());
        sig->setSigned(origSig->isSigned());
        sig->setFactor(origSig->getFactor());
        sig->setOffset(origSig->getOffset());
        sig->setMin(origSig->getMin());
        sig->setMax(origSig->getMax());
        sig->setUnit(origSig->getUnit());
        sig->setReceivers(origSig->getReceivers());
        sig->setDescription(origSig->getDescription());
        sig->setSendType(origSig->getSendType());
        sig->setInitialValue(origSig->getInitialValue());
        sig->setInvalidValueHex(origSig->getInvalidValueHex());
        sig->setInactiveValueHex(origSig->getInactiveValueHex());
        sig->setValueTable(origSig->getValueTable());
        if (origSig->hasRawRange()) {
            sig->setRawRange(origSig->getRawMin(), origSig->getRawMax());
        }

        m_currentMessage->addSignal(sig);
    }

    populateMessageTree();
    populateSignalTable(m_currentMessage);
    m_signalLayout->setMessage(m_currentMessage);
    markDirty();
}

void MainWindow::renameSignalPrefix(int row)
{
    if (!m_currentMessage) {
        return;
    }

    PrefixCopyOptions opt;
    if (!askPrefixCopyOptions(this, tr("修改前缀（信号）"), false, &opt)) {
        return;
    }
    if (!opt.applyToSignalNames) {
        return;
    }

    // 若 row 在选中集合里，则修改选中集合；否则只修改 row
    std::set<int> selectedRows;
    for (QTableWidgetItem *it : m_signalTable->selectedItems()) {
        if (it) {
            selectedRows.insert(it->row());
        }
    }
    if (selectedRows.empty()) {
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    } else if (selectedRows.find(row) == selectedRows.end()) {
        selectedRows.clear();
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    }

    // collect unique pointers
    std::vector<CanSignal *> sigs;
    std::set<CanSignal *> sigSet;
    for (int r : selectedRows) {
        if (r < 0 || r >= m_signalTable->rowCount()) continue;
        QTableWidgetItem *nameItem = m_signalTable->item(r, 0);
        if (!nameItem) continue;
        void *ptr = nameItem->data(Qt::UserRole).value<void *>();
        CanSignal *sig = static_cast<CanSignal *>(ptr);
        if (!sig) continue;
        if (sigSet.insert(sig).second) {
            sigs.push_back(sig);
        }
    }
    if (sigs.empty()) {
        return;
    }

    for (CanSignal *sig : sigs) {
        const QString baseSigName = sig->getName().isEmpty() ? QStringLiteral("Signal") : sig->getName();
        const QString desired = applyPrefixRule(baseSigName, opt.prefix);
        sig->setName(uniqueSignalName(m_currentMessage, sig, desired));
    }

    populateMessageTree();
    populateSignalTable(m_currentMessage);
    m_signalLayout->setMessage(m_currentMessage);
    if (m_currentSignal) {
        populateSignalDetails(m_currentSignal);
    }
    markDirty();
}

void MainWindow::addMessage()
{
    if (!m_dbcParser) {
        return;
    }

    QList<CanMessage*> &messages = m_dbcParser->messages();
    quint32 maxId = 0;
    for (CanMessage *msg : messages) {
        if (msg && msg->getId() > maxId) {
            maxId = msg->getId();
        }
    }

    CanMessage *message = new CanMessage();
    message->setId(maxId + 1);
    message->setName(QStringLiteral("NewMessage_%1").arg(messages.size() + 1));
    message->setLength(8);

    m_dbcParser->addMessage(message);

    populateMessageTree();
    markDirty();

    // Select the newly added message
    const int topCount = m_messageTree->topLevelItemCount();
    if (topCount > 0) {
        QTreeWidgetItem *last = m_messageTree->topLevelItem(topCount - 1);
        m_messageTree->setCurrentItem(last);
    }
}

void MainWindow::deleteMessage()
{
    if (!m_dbcParser) {
        return;
    }
    // 批量删除：Shift 多选后，删除全部被选中的报文
    const QList<QTreeWidgetItem *> items = m_messageTree->selectedItems();
    if (items.isEmpty()) {
        QTreeWidgetItem *cur = m_messageTree->currentItem();
        if (!cur) {
            return;
        }
        m_messageTree->clearSelection();
        cur->setSelected(true);
    }

    const QList<QTreeWidgetItem *> items2 = m_messageTree->selectedItems();
    if (items2.isEmpty()) {
        return;
    }

    std::set<CanMessage *> selectedMessages;
    QStringList nameList;
    for (QTreeWidgetItem *it : items2) {
        if (!it) continue;
        QTreeWidgetItem *msgItem = it->parent() != nullptr ? it->parent() : it;
        void *ptr = msgItem->data(0, Qt::UserRole).value<void *>();
        CanMessage *message = static_cast<CanMessage *>(ptr);
        if (!message) continue;
        if (selectedMessages.insert(message).second) {
            nameList.append(message->getName());
        }
    }
    if (selectedMessages.empty()) {
        return;
    }

    QString preview;
    int shown = 0;
    for (const QString &n : nameList) {
        if (shown >= 3) break;
        if (!preview.isEmpty()) preview += "，";
        preview += n;
        ++shown;
    }
    const QString retText = tr("将删除 %1 个报文%2。此操作不可撤销。")
        .arg(static_cast<int>(selectedMessages.size()))
        .arg(preview.isEmpty() ? QString() : tr("（前几个：%1）").arg(preview));

    const auto ret = QMessageBox::question(this, tr("删除报文"), retText);
    if (ret != QMessageBox::Yes) {
        return;
    }

    for (CanMessage *m : selectedMessages) {
        m_dbcParser->removeMessage(m);
    }
    populateMessageTree();
    clearViews();
    markDirty();
}

void MainWindow::addSignal()
{
    if (!m_currentMessage) {
        return;
    }

    // Find first free bit position in message
    const int msgLen = m_currentMessage->getLength();
    const int totalBits = msgLen * 8;
    QVector<bool> used(totalBits, false);
    const QList<CanSignal*> signalList = m_currentMessage->getSignals();
    for (CanSignal *sig : signalList) {
        if (!sig) {
            continue;
        }
        const bool motorola = (sig->getByteOrder() == 0);
        int startBit = sig->getStartBit();
        int length = sig->getLength();
        if (length <= 0) {
            continue;
        }
        if (motorola) {
            int bitIndex = startBit;
            for (int k = 0; k < length; ++k) {
                if (bitIndex >= 0 && bitIndex < totalBits) {
                    used[bitIndex] = true;
                }
                if (bitIndex % 8 == 0) {
                    bitIndex += 15;
                } else {
                    bitIndex -= 1;
                }
            }
        } else {
            for (int k = 0; k < length; ++k) {
                int bitIndex = startBit + k;
                if (bitIndex >= 0 && bitIndex < totalBits) {
                    used[bitIndex] = true;
                }
            }
        }
    }

    int freeBit = -1;
    for (int i = 0; i < totalBits; ++i) {
        if (!used[i]) {
            freeBit = i;
            break;
        }
    }
    if (freeBit < 0) {
        QMessageBox::warning(this, tr("添加信号"), tr("当前报文中没有可用的空闲位。"));
        return;
    }

    CanSignal *signal = new CanSignal();
    signal->setName(QStringLiteral("NewSignal_%1").arg(signalList.size() + 1));
    signal->setStartBit(freeBit);
    signal->setLength(1);
    signal->setFactor(1.0);
    signal->setOffset(0.0);
    signal->setMin(0.0);
    signal->setMax(1.0);

    m_currentMessage->addSignal(signal);

    populateMessageTree();
    populateSignalTable(m_currentMessage);
    m_signalLayout->setMessage(m_currentMessage);
    markDirty();
}

void MainWindow::deleteSignal()
{
    if (!m_currentMessage) {
        return;
    }
    int row = m_signalTable->currentRow();
    if (row < 0) {
        // 若当前行未设置但存在多选，取一个选中行来触发批量删除
        for (QTableWidgetItem *it : m_signalTable->selectedItems()) {
            if (it) {
                row = it->row();
                break;
            }
        }
    }
    if (row < 0) {
        return;
    }
    deleteSignalAtRow(row);
}

void MainWindow::deleteSignalAtRow(int row)
{
    if (!m_currentMessage) {
        return;
    }

    // Shift 多选后：如果 row 在选中行集合里，则删除所有选中信号；否则只删除该 row。
    std::set<int> selectedRows;
    for (QTableWidgetItem *it : m_signalTable->selectedItems()) {
        if (it) {
            selectedRows.insert(it->row());
        }
    }
    if (selectedRows.empty()) {
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    } else if (selectedRows.find(row) == selectedRows.end()) {
        selectedRows.clear();
        if (row < 0 || row >= m_signalTable->rowCount()) {
            return;
        }
        selectedRows.insert(row);
    }

    // 收集要删除的信号指针
    std::vector<CanSignal *> sigsToDelete;
    std::set<CanSignal *> sigSet;
    for (int r : selectedRows) {
        if (r < 0 || r >= m_signalTable->rowCount()) {
            continue;
        }
        QTableWidgetItem *nameItem = m_signalTable->item(r, 0);
        if (!nameItem) {
            continue;
        }
        void *ptr = nameItem->data(Qt::UserRole).value<void *>();
        CanSignal *sig = static_cast<CanSignal *>(ptr);
        if (!sig) {
            continue;
        }
        if (sigSet.insert(sig).second) {
            sigsToDelete.push_back(sig);
        }
    }
    if (sigsToDelete.empty()) {
        return;
    }

    // 确认提示（一次性）
    QString preview;
    int shown = 0;
    for (CanSignal *s : sigsToDelete) {
        if (!s) continue;
        if (shown >= 3) break;
        if (!preview.isEmpty()) preview += "，";
        preview += s->getName();
        ++shown;
    }
    const QString retText = tr("将删除 %1 个信号%2。此操作不可撤销。")
        .arg(static_cast<int>(sigsToDelete.size()))
        .arg(preview.isEmpty() ? QString() : tr("（前几个：%1）").arg(preview));
    const auto ret = QMessageBox::question(this, tr("删除信号"), retText);
    if (ret != QMessageBox::Yes) {
        return;
    }

    // 实际删除
    for (CanSignal *sig : sigsToDelete) {
        if (!sig) continue;
        if (m_currentSignal == sig) {
            m_currentSignal = nullptr;
        }
        m_currentMessage->removeSignal(sig);
        delete sig;
    }

    populateMessageTree();
    populateSignalTable(m_currentMessage);
    m_signalLayout->setMessage(m_currentMessage);
    m_signalLayout->setHighlightedSignal(m_currentSignal);
    m_signalDetails->clear();
    m_valueTable->clear();
    if (m_applyValueTableButton) {
        m_applyValueTableButton->setEnabled(false);
    }
    markDirty();
}

void MainWindow::onSignalSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_signalTable->selectedItems();
    
    if (selectedItems.isEmpty()) {
        m_currentSignal = nullptr;
        m_signalLayout->setHighlightedSignal(nullptr);
        m_signalDetails->clear();
        m_valueTable->clear();
        m_detailsStack->setCurrentIndex(0);
        return;
    }
    
    QTableWidgetItem *item = selectedItems.first();
    void *data = item->data(Qt::UserRole).value<void*>();
    m_currentSignal = static_cast<CanSignal*>(data);
    m_signalLayout->setMessage(m_currentMessage);
    m_signalLayout->setHighlightedSignal(m_currentSignal);
    m_detailsStack->setCurrentIndex(1);
    populateSignalDetails(m_currentSignal);
}

void MainWindow::clearViews()
{
    m_signalTable->setRowCount(0);
    m_signalDetails->clear();
    m_valueTable->clear();
    m_signalLayout->setMessage(nullptr);
    m_signalLayout->setHighlightedSignal(nullptr);
    m_detailsStack->setCurrentIndex(0);
    m_currentMessage = nullptr;
    m_currentSignal = nullptr;
    if (m_applyValueTableButton) {
        m_applyValueTableButton->setEnabled(false);
    }
}

void MainWindow::onMessageTreeItemChanged(QTreeWidgetItem *item, int column)
{
    if (!item || m_isUpdatingMessageTree) {
        return;
    }

    // Top-level items are messages, children are signals
    if (item->parent() != nullptr) {
        // For now, keep signal rows in tree read-only (name编辑在表格中处理)
        return;
    }

    void *ptr = item->data(0, Qt::UserRole).value<void *>();
    CanMessage *message = static_cast<CanMessage *>(ptr);
    if (!message) {
        return;
    }

    const QString text = item->text(column).trimmed();

    m_isUpdatingMessageTree = true;

    switch (column) {
    case 0: { // ID
        QString idText = text;
        bool ok = false;
        quint32 id = 0;
        if (idText.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            id = idText.toUInt(&ok, 16);
        } else {
            id = idText.toUInt(&ok, 10);
        }
        if (!ok) {
            item->setText(0, message->getFormattedId());
            m_statusLabel->setText(tr("无效的报文ID：%1").arg(text));
        } else {
            message->setId(id);
            item->setText(0, message->getFormattedId());
            markDirty();
        }
        break;
    }
    case 1: // Name
        message->setName(text);
        markDirty();
        break;
    case 2: { // Length
        bool ok = false;
        int len = text.toInt(&ok);
        if (!ok || len < 0) {
            item->setText(2, QString::number(message->getLength()));
            m_statusLabel->setText(tr("无效的报文长度：%1").arg(text));
        } else {
            message->setLength(len);
            item->setText(2, QString::number(message->getLength()));
            if (message == m_currentMessage) {
                populateSignalTable(m_currentMessage);
                m_signalLayout->setMessage(m_currentMessage);
            }
            markDirty();
        }
        break;
    }
    case 3: // Transmitter
        message->setTransmitter(text);
        markDirty();
        break;
    default:
        break;
    }

    m_isUpdatingMessageTree = false;
}

void MainWindow::applySaveSnapshot()
{
    createSnapshotFromCurrent();
    m_isDirty = false;
}

void MainWindow::markDirty()
{
    if (!m_isDirty) {
        m_isDirty = true;
        m_statusLabel->setText(tr("已修改（未保存）"));
    }
}

void MainWindow::clearSavedSnapshot()
{
    for (CanMessage *msg : m_savedMessages) {
        if (!msg) {
            continue;
        }
        for (CanSignal *sig : msg->getSignals()) {
            delete sig;
        }
        delete msg;
    }
    m_savedMessages.clear();
    m_savedVersion.clear();
    m_savedBusType.clear();
    m_savedNodes.clear();
    m_savedDocumentTitle.clear();
    m_savedChangeHistory.clear();
    m_savedGlobalValueTables.clear();
}

QList<CanMessage*> MainWindow::cloneMessages(const QList<CanMessage*> &source) const
{
    QList<CanMessage*> result;
    for (CanMessage *origMsg : source) {
        if (!origMsg) {
            continue;
        }
        CanMessage *msg = new CanMessage();
        msg->setId(origMsg->getId());
        msg->setName(origMsg->getName());
        msg->setLength(origMsg->getLength());
        msg->setTransmitter(origMsg->getTransmitter());
        msg->setCycleTime(origMsg->getCycleTime());
        msg->setFrameFormat(origMsg->getFrameFormat());
        msg->setSendType(origMsg->getSendType());
        msg->setCycleTimeFast(origMsg->getCycleTimeFast());
        msg->setNrOfRepetitions(origMsg->getNrOfRepetitions());
        msg->setDelayTime(origMsg->getDelayTime());
        msg->setComment(origMsg->getComment());
        msg->setMessageType(origMsg->getMessageType());
        msg->setReceivers(origMsg->getReceivers());

        for (CanSignal *origSig : origMsg->getSignals()) {
            if (!origSig) {
                continue;
            }
            CanSignal *sig = new CanSignal();
            sig->setName(origSig->getName());
            sig->setStartBit(origSig->getStartBit());
            sig->setLength(origSig->getLength());
            sig->setByteOrder(origSig->getByteOrder());
            sig->setSigned(origSig->isSigned());
            sig->setFactor(origSig->getFactor());
            sig->setOffset(origSig->getOffset());
            sig->setMin(origSig->getMin());
            sig->setMax(origSig->getMax());
            sig->setUnit(origSig->getUnit());
            sig->setReceivers(origSig->getReceivers());
            sig->setDescription(origSig->getDescription());
            sig->setSendType(origSig->getSendType());
            sig->setInitialValue(origSig->getInitialValue());
            sig->setInvalidValueHex(origSig->getInvalidValueHex());
            sig->setInactiveValueHex(origSig->getInactiveValueHex());
            sig->setValueTable(origSig->getValueTable());
            if (origSig->hasRawRange()) {
                sig->setRawRange(origSig->getRawMin(), origSig->getRawMax());
            }
            msg->addSignal(sig);
        }

        result.append(msg);
    }
    return result;
}

void MainWindow::createSnapshotFromCurrent()
{
    if (!m_dbcParser) {
        return;
    }
    clearSavedSnapshot();

    m_savedMessages = cloneMessages(m_dbcParser->getMessages());
    m_savedVersion = m_dbcParser->getVersion();
    m_savedBusType = m_dbcParser->getBusType();
    m_savedNodes = m_dbcParser->getNodes();
    m_savedDocumentTitle = m_dbcParser->getDocumentTitle();
    m_savedChangeHistory = m_dbcParser->getChangeHistory();
    m_savedGlobalValueTables = m_dbcParser->getGlobalValueTables();
}

void MainWindow::restoreSnapshotToCurrent()
{
    if (!m_dbcParser || m_savedMessages.isEmpty()) {
        return;
    }

    // 清空当前解析器并用快照重建
    m_dbcParser->clear();
    for (CanMessage *msg : cloneMessages(m_savedMessages)) {
        m_dbcParser->addMessage(msg);
    }

    // 恢复其他元数据
    // 这些字段在 DbcParser 中是私有成员，只能通过当前公开接口在导出时使用；
    // 这里保持它们只在快照中用于导出，不强行写回 parser。

    populateMessageTree();
    clearViews();
    m_isDirty = false;
    m_statusLabel->setText(tr("已恢复到上次保存状态"));
}

void MainWindow::showValidationErrorsIfAny()
{
    const ValidationResult result = validateMessages(m_dbcParser->getMessages());
    if (!result.ok && !result.errors.isEmpty()) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("导入数据校验"));
        msgBox.setIcon(QMessageBox::Warning);
        msgBox.setText(tr("导入数据存在以下问题："));
        msgBox.setDetailedText(result.errors.join(QStringLiteral("\n")));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About DBC Viewer",
        "<h3>DBC Viewer</h3>"
        "<p>A Qt-based application for viewing and analyzing DBC (Database CAN) files.</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Parse and display CAN messages and signals</li>"
        "<li>View signal properties and value tables</li>"
        "<li>Tree view of messages and signals</li>"
        "<li>Detailed signal information</li>"
        "<li>Drag and drop DBC files</li>"
        "</ul>"
        "<p>Built with Qt C++</p>");
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString fileName = urls.first().toLocalFile();
            if (fileName.endsWith(".dbc", Qt::CaseInsensitive)
                || fileName.endsWith(".xlsx", Qt::CaseInsensitive)) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString fileName = urls.first().toLocalFile();
            if (fileName.endsWith(".dbc", Qt::CaseInsensitive)) {
                loadDbcFile(fileName);
                event->acceptProposedAction();
                return;
            }
            if (fileName.endsWith(".xlsx", Qt::CaseInsensitive)) {
                DbcExcelConverter::ImportResult importResult;
                QString errorMessage;
                if (!DbcExcelConverter::importFromExcel(fileName, importResult, &errorMessage)) {
                    QMessageBox::critical(this, "Open Failed", errorMessage);
                    event->ignore();
                    return;
                }
                m_dbcParser->loadFromExcelImport(importResult);
                m_currentDbcPath = fileName;
                m_fileLabel->setText(QString("File: %1").arg(QFileInfo(fileName).fileName()));
                m_statusLabel->setText(QString("Loaded %1 messages").arg(m_dbcParser->getMessages().size()));
                populateMessageTree();
                clearViews();
                showValidationErrorsIfAny();
                applySaveSnapshot();
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}
