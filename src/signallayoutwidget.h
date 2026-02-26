#ifndef SIGNALLAYOUTWIDGET_H
#define SIGNALLAYOUTWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QObject>

class CanMessage;
class CanSignal;

/**
 * CANoe-style bitfield layout view for a CAN message.
 * Shows signal names on the left and a grid of bits (bytes x 8) with
 * each cell colored by signal and displaying global bit index.
 */
class SignalLayoutWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SignalLayoutWidget(QWidget *parent = nullptr);

    /** Set the message to display; pass nullptr to clear. */
    void setMessage(CanMessage *message);
    /** Optionally highlight a specific signal (e.g. when selected). */
    void setHighlightedSignal(CanSignal *signal);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildLayout();
    void showSignalDetailDialog(CanSignal *signal);
    /** Returns global bit index for cell (row, col). row=byte, col=0..7, bit 0 = MSB of byte. */
    static int cellToBit(int row, int col);
    static void bitToCell(int bit, int *row, int *col);
    QColor colorForSignal(int signalIndex) const;

    QTableWidget *m_table;
    CanMessage *m_message;
    CanSignal *m_highlightedSignal;
    QList<QColor> m_signalColors;
};

#endif // SIGNALLAYOUTWIDGET_H
