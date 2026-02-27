#include "canmessage.h"

CanMessage::CanMessage()
    : m_id(0)
    , m_length(0)
    , m_cycleTime(0)
    , m_cycleTimeFast(0)
    , m_nrOfRepetitions(0)
    , m_delayTime(0)
{
}

CanSignal* CanMessage::getSignal(const QString &name) const
{
    for (CanSignal *signal : m_signals) {
        if (signal->getName() == name) {
            return signal;
        }
    }
    return nullptr;
}

void CanMessage::addSignal(CanSignal *signal)
{
    if (signal) {
        m_signals.append(signal);
    }
}

void CanMessage::removeSignal(CanSignal *signal)
{
    if (!signal) {
        return;
    }
    m_signals.removeAll(signal);
}

void CanMessage::insertSignal(int index, CanSignal *signal)
{
    if (!signal) {
        return;
    }
    if (index < 0 || index > m_signals.size()) {
        m_signals.append(signal);
    } else {
        m_signals.insert(index, signal);
    }
}

QString CanMessage::getFormattedId() const
{
    return QString("0x%1").arg(m_id, 0, 16).toUpper();
}

QString CanMessage::getFormattedLength() const
{
    return QString("%1 bytes").arg(m_length);
}
