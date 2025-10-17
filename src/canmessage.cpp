#include "canmessage.h"

CanMessage::CanMessage()
    : m_id(0)
    , m_length(0)
    , m_cycleTime(0)
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

QString CanMessage::getFormattedId() const
{
    return QString("0x%1").arg(m_id, 0, 16).toUpper();
}

QString CanMessage::getFormattedLength() const
{
    return QString("%1 bytes").arg(m_length);
}
