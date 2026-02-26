#include "signallayoutwidget.h"
#include "canmessage.h"
#include "cansignal.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QColor>

namespace {

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
};

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

QColor SignalLayoutWidget::colorForSignal(int signalIndex) const
{
    if (signalIndex < 0 || signalIndex >= m_signalColors.size()) {
        return QColor(0xE0, 0xE0, 0xE0);
    }
    return m_signalColors.at(signalIndex);
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

    // Columns: signal name (for this row) + 8 bits per byte -> we have dlc rows, each row is one byte
    // So we need one column for "signal name" and 8 columns for bits. But each row is one byte, so we have dlc rows.
    // Left column: show which signal "starts" or is primary in that row (first signal that has a bit in this row)
    const int nameCol = 0;
    const int firstBitCol = 1;
    const int totalCols = firstBitCol + 8;

    m_table->setColumnCount(totalCols);
    m_table->setRowCount(dlc);

    QStringList headers;
    headers << "" << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7";
    m_table->setHorizontalHeaderLabels(headers);

    // Build map: (row, col) -> signal index (-1 if no signal, else index in signals)
    // DBC: Intel (0) = LSB first, startBit is LSB; Motorola (1) = MSB first, startBit is MSB.
    // Grid: row = byte index, col 0 = MSB, col 7 = LSB. So (row,col) Intel bit = row*8+(7-col), Motorola bit = row*8+col.
    QVector<QVector<int>> cellSignal(dlc, QVector<int>(8, -1));
    for (int i = 0; i < signalList.size(); ++i) {
        CanSignal *sig = signalList.at(i);
        const int startBit = sig->getStartBit();
        const int length = sig->getLength();
        const bool motorola = (sig->getByteOrder() != 0);
        for (int k = 0; k < length; ++k) {
            const int bitIndex = startBit + k;
            int r, c;
            if (motorola) {
                r = bitIndex / 8;
                c = bitIndex % 8;
            } else {
                r = bitIndex / 8;
                c = 7 - (bitIndex % 8);
            }
            if (r >= 0 && r < dlc && c >= 0 && c < 8) {
                cellSignal[r][c] = i;
            }
        }
    }

    for (int row = 0; row < dlc; ++row) {
        // Signal name column: show the first signal that has a bit in this row
        QString nameForRow;
        int firstSigInRow = -1;
        for (int col = 0; col < 8; ++col) {
            int si = cellSignal[row][col];
            if (si >= 0) {
                if (firstSigInRow < 0 || si < firstSigInRow) {
                    firstSigInRow = si;
                }
            }
        }
        if (firstSigInRow >= 0) {
            nameForRow = signalList.at(firstSigInRow)->getName();
        }

        QTableWidgetItem *nameItem = new QTableWidgetItem(nameForRow);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        if (firstSigInRow >= 0) {
            QColor nameBg = colorForSignal(firstSigInRow);
            if (signalList.at(firstSigInRow) == m_highlightedSignal) {
                nameBg = nameBg.darker(120);
            }
            nameItem->setBackground(nameBg);
        } else {
            nameItem->setBackground(QColor(0xF5, 0xF5, 0xF5));
        }
        m_table->setItem(row, nameCol, nameItem);

        for (int col = 0; col < 8; ++col) {
            const int globalBit = cellToBit(row, col);
            const int sigIndex = cellSignal[row][col];

            QTableWidgetItem *item = new QTableWidgetItem(QString::number(globalBit));
            item->setTextAlignment(Qt::AlignCenter);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_table->setItem(row, firstBitCol + col, item);

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
    m_table->horizontalHeader()->setSectionResizeMode(nameCol, QHeaderView::Stretch);
}
