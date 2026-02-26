#include "signallayoutwidget.h"
#include "canmessage.h"
#include "cansignal.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QColor>
#include <QToolTip>
#include <QMouseEvent>
#include <QEvent>
#include <QApplication>
#include <QCursor>
#include <QDialog>
#include <QTabWidget>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFont>
#include <QtGlobal>
#include <cmath>
#include <limits>

namespace {

const int kMaxSignalNameChars = 12;

const QList<QColor> kDefaultColors = {
    QColor(0x8B, 0xC3, 0x4A),  // light green
    QColor(0x42, 0xA5, 0xF5),  // blue
    QColor(0xAB, 0x47, 0xBC),  // purple
    QColor(0xFF, 0x98, 0x00),  // orange
    QColor(0xEC, 0x40, 0x76),  // pink
    QColor(0x26, 0xA6, 0x9A),  // teal
    QColor(0xFF, 0xCA, 0x28),  // amber
    QColor(0x7E, 0x57, 0xC2),  // deep purple
    QColor(0xEF, 0x53, 0x50),  // red
    QColor(0x66, 0xBB, 0x6A),  // green
    QColor(0x5C, 0x6B, 0xC0),  // indigo
    QColor(0x26, 0xC6, 0xDA),  // cyan
    QColor(0xD4, 0xE1, 0x57),  // lime
    QColor(0xFF, 0x70, 0x43),  // deep orange
    QColor(0xBC, 0xAA, 0xA4),  // brown
};

const int kSignalIndexRole = Qt::UserRole + 1;

} // namespace

SignalLayoutWidget::SignalLayoutWidget(QWidget *parent)
    : QWidget(parent)
    , m_table(new QTableWidget(this))
    , m_message(nullptr)
    , m_highlightedSignal(nullptr)
{
    m_signalColors = kDefaultColors;
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_table);

    m_table->setAlternatingRowColors(false);
    m_table->setShowGrid(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setMouseTracking(true);
    m_table->viewport()->setMouseTracking(true);
    m_table->viewport()->installEventFilter(this);
}

int SignalLayoutWidget::cellToBit(int row, int col)
{
    return row * 8 + col;
}

void SignalLayoutWidget::bitToCell(int bit, int *row, int *col)
{
    if (row) *row = bit / 8;
    if (col) *col = bit % 8;
}

bool SignalLayoutWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_table->viewport()) {
        return QWidget::eventFilter(watched, event);
    }
    QMouseEvent *mouseEvent = nullptr;
    if (event->type() == QEvent::MouseMove) {
        QPoint pos = static_cast<QMouseEvent *>(event)->pos();
        QTableWidgetItem *item = m_table->itemAt(pos);
        QString tip = item ? item->data(Qt::UserRole).toString() : QString();
        if (tip.isEmpty()) {
            QToolTip::hideText();
            QToolTip::showText(QPoint(-10000, -10000), QString(), m_table);
        } else {
            QPoint globalPos = m_table->viewport()->mapToGlobal(pos);
            QToolTip::showText(globalPos + QPoint(12, 20), tip, m_table);
        }
    } else if (event->type() == QEvent::Leave) {
        QToolTip::hideText();
        QToolTip::showText(QPoint(-10000, -10000), QString(), m_table);
    } else if (event->type() == QEvent::MouseButtonPress) {
        mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPoint pos = mouseEvent->pos();
            QTableWidgetItem *item = m_table->itemAt(pos);
            QString tip = item ? item->data(Qt::UserRole).toString() : QString();
            if (!tip.isEmpty()) {
                QPoint globalPos = m_table->viewport()->mapToGlobal(pos);
                QToolTip::showText(globalPos + QPoint(12, 20), tip, m_table);
            }
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            QPoint pos = mouseEvent->pos();
            QTableWidgetItem *item = m_table->itemAt(pos);
            QString tip = item ? item->data(Qt::UserRole).toString() : QString();
            if (!tip.isEmpty()) {
                QPoint globalPos = m_table->viewport()->mapToGlobal(pos);
                QToolTip::showText(globalPos + QPoint(12, 20), tip, m_table);
            }
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && m_message) {
            QPoint pos = mouseEvent->pos();
            QTableWidgetItem *item = m_table->itemAt(pos);
            const int sigIndex = item ? item->data(kSignalIndexRole).toInt() : -1;
            if (sigIndex >= 0 && sigIndex < m_message->getSignals().size()) {
                CanSignal *signal = m_message->getSignals().at(sigIndex);
                showSignalDetailDialog(signal);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

QColor SignalLayoutWidget::colorForSignal(int signalIndex) const
{
    if (signalIndex < 0 || m_signalColors.isEmpty()) {
        return QColor(0xE0, 0xE0, 0xE0);
    }
    return m_signalColors.at(signalIndex % m_signalColors.size());
}

void SignalLayoutWidget::setMessage(CanMessage *message)
{
    m_message = message;
    buildLayout();
}

void SignalLayoutWidget::setHighlightedSignal(CanSignal *signal)
{
    m_highlightedSignal = signal;
    buildLayout();
}

void SignalLayoutWidget::showSignalDetailDialog(CanSignal *signal)
{
    if (!signal) {
        return;
    }
    auto maskForLength = [](int length) -> quint64 {
        if (length <= 0) return 0;
        if (length >= 64) return std::numeric_limits<quint64>::max();
        return (quint64(1) << length) - 1;
    };
    const quint64 initialValue = static_cast<quint64>(std::llround(signal->getInitialValue())) & maskForLength(signal->getLength());
    const QString byteOrderText = signal->getByteOrder() == 0 ? "Intel LSB" : "Motorola MSB";
    const QString receivers = signal->getReceiversAsString().isEmpty() ? "N/A" : signal->getReceiversAsString();
    const QString sendType = signal->getSendType().isEmpty() ? "N/A" : signal->getSendType();
    const QString invalidValue = signal->getInvalidValueHex().isEmpty() ? "-" : signal->getInvalidValueHex();
    const QString inactiveValue = signal->getInactiveValueHex().isEmpty() ? "-" : signal->getInactiveValueHex();

    const QString details = QString(
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

    QString valueTableText;
    const QMap<int, QString> valueTable = signal->getValueTable();
    if (valueTable.isEmpty()) {
        valueTableText = "No value table defined for this signal.";
    } else {
        valueTableText = "Value Table:\n\n";
        for (auto it = valueTable.begin(); it != valueTable.end(); ++it) {
            valueTableText += QString("%1: %2\n").arg(it.key()).arg(it.value());
        }
    }

    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(QString("Signal: %1").arg(signal->getName()));
    dialog->setMinimumSize(480, 400);
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    QTabWidget *tabs = new QTabWidget(dialog);
    QTextEdit *propsEdit = new QTextEdit(dialog);
    propsEdit->setReadOnly(true);
    propsEdit->setFont(QFont("Courier", 10));
    propsEdit->setPlainText(details);
    QTextEdit *valueEdit = new QTextEdit(dialog);
    valueEdit->setReadOnly(true);
    valueEdit->setFont(QFont("Courier", 10));
    valueEdit->setPlainText(valueTableText);
    tabs->addTab(propsEdit, "Properties");
    tabs->addTab(valueEdit, "Value Table");
    layout->addWidget(tabs);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, dialog);
    connect(box, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(box);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void SignalLayoutWidget::buildLayout()
{
    m_table->clear();
    m_table->setRowCount(0);
    m_table->setColumnCount(0);

    if (!m_message) {
        return;
    }

    const int dlc = m_message->getLength();
    if (dlc <= 0) {
        return;
    }

    const QList<CanSignal*> signalList = m_message->getSignals();

    // Grid: 8 columns (0-7), one row per byte. Signal names go inside the grid (in each signal's first cell).
    const int totalCols = 8;
    m_table->setColumnCount(totalCols);
    m_table->setRowCount(dlc);

    QStringList headers;
    headers << "7" << "6" << "5" << "4" << "3" << "2" << "1" << "0";
    m_table->setHorizontalHeaderLabels(headers);

    // DBC 约定：@0 = Motorola（大端，startBit 为 MSB），@1 = Intel（小端，startBit 为 LSB）
    // 物理位 c：0=LSB、7=MSB。显示列 0 对应 bit 7（左 MSB），显示列 7 对应 bit 0（右 LSB），故 displayCol = 7 - c
    QVector<QVector<int>> cellSignal(dlc, QVector<int>(8, -1));
    QHash<int, QPair<int, int>> signalStartCell;
    for (int i = 0; i < signalList.size(); ++i) {
        CanSignal *sig = signalList.at(i);
        const int startBit = sig->getStartBit();
        const int length = sig->getLength();
        const bool motorola = (sig->getByteOrder() == 0);
        int startRow = -1, startCol = -1;
        int bitIndex = startBit;
        for (int k = 0; k < length; ++k) {
            int r, c;
            if (motorola) {
                r = bitIndex / 8;
                c = bitIndex % 8;
                if (k == 0) {
                    startRow = r;
                    startCol = 7 - c;
                }
                if (r >= 0 && r < dlc && c >= 0 && c < 8) {
                    cellSignal[r][7 - c] = i;
                }
                if (bitIndex % 8 == 0) {
                    bitIndex += 15;
                } else {
                    bitIndex -= 1;
                }
            } else {
                bitIndex = startBit + k;
                r = bitIndex / 8;
                c = bitIndex % 8;
                if (k == 0) {
                    startRow = r;
                    startCol = 7 - c;
                }
                if (r >= 0 && r < dlc && c >= 0 && c < 8) {
                    cellSignal[r][7 - c] = i;
                }
            }
        }
        if (startRow >= 0 && startCol >= 0) {
            signalStartCell[i] = qMakePair(startRow, startCol);
        }
    }

    for (int row = 0; row < dlc; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int globalBit = row * 8 + (7 - col);
            const int sigIndex = cellSignal[row][col];

            QString cellText;
            QString tooltip;
            if (sigIndex >= 0) {
                QPair<int, int> start = signalStartCell.value(sigIndex, qMakePair(-1, -1));
                if (start.first == row && start.second == col) {
                    QString fullName = signalList.at(sigIndex)->getName();
                    if (fullName.length() > kMaxSignalNameChars) {
                        cellText = fullName.left(kMaxSignalNameChars) + QString("…");
                        tooltip = fullName;
                    } else {
                        cellText = fullName;
                    }
                } else {
                    cellText = QString::number(globalBit);
                }
                if (tooltip.isEmpty() && sigIndex >= 0) {
                    tooltip = signalList.at(sigIndex)->getName();
                }
            } else {
                cellText = QString::number(globalBit);
            }

            QTableWidgetItem *item = new QTableWidgetItem(cellText);
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, tooltip);
            item->setData(kSignalIndexRole, sigIndex);
            item->setToolTip(QString());
            m_table->setItem(row, col, item);

            if (sigIndex >= 0) {
                QColor bg = colorForSignal(sigIndex);
                if (signalList.at(sigIndex) == m_highlightedSignal) {
                    bg = bg.darker(120);
                }
                item->setBackground(bg);
            } else {
                item->setBackground(QColor(0xF5, 0xF5, 0xF5));
            }
        }
    }

    m_table->resizeColumnsToContents();
    for (int c = 0; c < 8; ++c) {
        m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);
    }
}
