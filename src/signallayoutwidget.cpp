#include "signallayoutwidget.h"
#include "canmessage.h"
#include "cansignal.h"
#include <QHeaderView>
#include <QVBoxLayout>
#include <QColor>

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
    headers << "0" << "1" << "2" << "3" << "4" << "5" << "6" << "7";
    m_table->setHorizontalHeaderLabels(headers);

    // Build map: (row, col) -> signal index (-1 if no signal)
    QVector<QVector<int>> cellSignal(dlc, QVector<int>(8, -1));
    // Start cell of each signal (row, col) for showing name
    QHash<int, QPair<int, int>> signalStartCell;
    for (int i = 0; i < signalList.size(); ++i) {
        CanSignal *sig = signalList.at(i);
        const int startBit = sig->getStartBit();
        const int length = sig->getLength();
        const bool motorola = (sig->getByteOrder() != 0);
        int startRow = -1, startCol = -1;
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
            if (k == 0) {
                startRow = r;
                startCol = c;
            }
            if (r >= 0 && r < dlc && c >= 0 && c < 8) {
                cellSignal[r][c] = i;
            }
        }
        if (startRow >= 0 && startCol >= 0) {
            signalStartCell[i] = qMakePair(startRow, startCol);
        }
    }

    for (int row = 0; row < dlc; ++row) {
        for (int col = 0; col < 8; ++col) {
            const int globalBit = cellToBit(row, col);
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
            if (!tooltip.isEmpty()) {
                item->setToolTip(tooltip);
            }
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
