#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QFileDialog>
#include <QLabel>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include "dbcparser.h"
#include "dbcexcelconverter.h"
#include "dbcwriter.h"

class SignalLayoutWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void openFile();
    void exportToExcelByEcu();
    void exportToExcelSingleSheet();
    void exportToDbc();
    void onMessageSelectionChanged();
    void onSignalSelectionChanged();
    void onSignalTableHeaderClicked(int logicalIndex);
    void showAbout();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupStatusBar();
    void loadDbcFile(const QString &filePath);
    void populateMessageTree();
    void populateSignalTable(CanMessage *message);
    void populateSignalDetails(CanSignal *signal);
    void clearViews();
    void showValidationErrorsIfAny();
    
    // UI Components
    QWidget *m_centralWidget;
    QSplitter *m_mainSplitter;
    QSplitter *m_rightSplitter;
    
    // Left panel - Message tree
    QGroupBox *m_messageGroup;
    QTreeWidget *m_messageTree;
    
    // Right panel - Signal table and details
    QGroupBox *m_signalGroup;
    QTableWidget *m_signalTable;
    
    QGroupBox *m_detailsGroup;
    QStackedWidget *m_detailsStack;
    SignalLayoutWidget *m_signalLayout;
    QTabWidget *m_detailsTabs;
    QTextEdit *m_signalDetails;
    QTextEdit *m_valueTable;
    
    // Status bar
    QLabel *m_statusLabel;
    QLabel *m_fileLabel;
    
    // Data
    DbcParser *m_dbcParser;
    CanMessage *m_currentMessage;
    CanSignal *m_currentSignal;
    QString m_currentDbcPath;
    int m_signalTableSortColumn = -1;
    Qt::SortOrder m_signalTableSortOrder = Qt::AscendingOrder;
};

#endif // MAINWINDOW_H
