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

#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

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
    m_messageTree->sortByColumn(0, Qt::AscendingOrder);
    
    connect(m_messageTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onMessageSelectionChanged);
    
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
    m_signalTable->horizontalHeader()->setStretchLastSection(true);
    m_signalTable->horizontalHeader()->setSectionsClickable(true);

    connect(m_signalTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::onSignalSelectionChanged);
    connect(m_signalTable->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onSignalTableHeaderClicked);
    
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
    m_valueTable->setReadOnly(true);
    m_valueTable->setFont(QFont("Courier", 10));
    
    m_detailsTabs->addTab(m_signalDetails, "Properties");
    m_detailsTabs->addTab(m_valueTable, "Value Table");
    
    m_detailsStack->addWidget(m_signalLayout);   // index 0: message bitfield layout
    m_detailsStack->addWidget(m_detailsTabs);   // index 1: signal properties + value table
    detailsLayout->addWidget(m_detailsStack);
    
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
    } else {
        loadDbcFile(fileName);
    }
}

void MainWindow::exportToExcelByEcu()
{
    if (m_dbcParser->getMessages().isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to Excel.");
        return;
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
                                          m_dbcParser->getVersion(),
                                          m_dbcParser->getBusType(),
                                          m_dbcParser->getNodes(),
                                          m_dbcParser->getMessages(),
                                          m_dbcParser->getDocumentTitle(),
                                          m_dbcParser->getChangeHistory(),
                                          true,
                                          &errorMessage)) {
        QMessageBox::critical(this, "Export Failed", errorMessage);
        return;
    }

    m_statusLabel->setText(QString("Exported Excel (by ECU): %1").arg(QFileInfo(normalizedPath).fileName()));
}

void MainWindow::exportToExcelSingleSheet()
{
    if (m_dbcParser->getMessages().isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to Excel.");
        return;
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
                                          m_dbcParser->getVersion(),
                                          m_dbcParser->getBusType(),
                                          m_dbcParser->getNodes(),
                                          m_dbcParser->getMessages(),
                                          m_dbcParser->getDocumentTitle(),
                                          m_dbcParser->getChangeHistory(),
                                          false,
                                          &errorMessage)) {
        QMessageBox::critical(this, "Export Failed", errorMessage);
        return;
    }

    m_statusLabel->setText(QString("Exported Excel (single sheet): %1").arg(QFileInfo(normalizedPath).fileName()));
}

void MainWindow::exportToDbc()
{
    if (m_dbcParser->getMessages().isEmpty()) {
        QMessageBox::warning(this, "Export", "Please load a DBC file before exporting to DBC.");
        return;
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
                          m_dbcParser->getVersion(),
                          m_dbcParser->getBusType(),
                          m_dbcParser->getNodes(),
                          m_dbcParser->getMessages(),
                          QString(),
                          m_dbcParser->getDocumentTitle(),
                          m_dbcParser->getChangeHistory(),
                          m_dbcParser->getGlobalValueTables(),
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
    } else {
        QMessageBox::critical(this, "Error", "Failed to parse DBC file!");
        m_statusLabel->setText("Error loading file");
    }
}

void MainWindow::populateMessageTree()
{
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
        
        // Add signals as child items
        for (CanSignal *signal : message->getSignals()) {
            QTreeWidgetItem *signalItem = new QTreeWidgetItem(item);
            signalItem->setText(0, signal->getName());
            signalItem->setText(1, QString("Bit %1").arg(signal->getStartBit()));
            signalItem->setText(2, QString("%1 bits").arg(signal->getLength()));
            signalItem->setText(3, signal->getUnit());
            signalItem->setText(4, signal->getReceiversAsString());
            
            // Store signal pointer in item data
            signalItem->setData(0, Qt::UserRole, QVariant::fromValue(static_cast<void*>(signal)));
        }
    }
    
    m_messageTree->collapseAll();
}

void MainWindow::populateSignalTable(CanMessage *message)
{
    if (!message) {
        m_signalTable->setRowCount(0);
        return;
    }
    
    QList<CanSignal*> signalList = message->getSignals();
    m_signalTable->setRowCount(signalList.size());
    
    for (int i = 0; i < signalList.size(); ++i) {
        CanSignal *signal = signalList[i];
        
        m_signalTable->setItem(i, 0, new QTableWidgetItem(signal->getName()));
        m_signalTable->setItem(i, 1, new QTableWidgetItem(QString::number(signal->getStartBit())));
        m_signalTable->setItem(i, 2, new QTableWidgetItem(QString::number(signal->getLength())));
        m_signalTable->setItem(i, 3, new QTableWidgetItem(QString::number(signal->getFactor())));
        m_signalTable->setItem(i, 4, new QTableWidgetItem(QString::number(signal->getOffset())));
        m_signalTable->setItem(i, 5, new QTableWidgetItem(QString::number(signal->getMin())));
        m_signalTable->setItem(i, 6, new QTableWidgetItem(QString::number(signal->getMax())));
        m_signalTable->setItem(i, 7, new QTableWidgetItem(signal->getUnit()));

        m_signalTable->item(i, 0)->setData(Qt::UserRole, QVariant::fromValue(static_cast<void *>(signal)));
        m_signalTable->item(i, 1)->setData(Qt::UserRole, signal->getStartBit());
        m_signalTable->item(i, 2)->setData(Qt::UserRole, signal->getLength());
        m_signalTable->item(i, 3)->setData(Qt::UserRole, signal->getFactor());
        m_signalTable->item(i, 4)->setData(Qt::UserRole, signal->getOffset());
        m_signalTable->item(i, 5)->setData(Qt::UserRole, signal->getMin());
        m_signalTable->item(i, 6)->setData(Qt::UserRole, signal->getMax());
    }
    
    m_signalTable->resizeColumnsToContents();
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
    const QString byteOrderText = signal->getByteOrder() == 0 ? "Intel LSB" : "Motorola MSB";
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
     .arg(signal->getStartBit())
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
        QString valueTableText = "Value Table:\n\n";
        for (auto it = valueTable.begin(); it != valueTable.end(); ++it) {
            valueTableText += QString("%1: %2\n").arg(it.key()).arg(it.value());
        }
        m_valueTable->setPlainText(valueTableText);
    }
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
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}
