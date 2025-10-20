#include "cansignal.h"

CanSignal::CanSignal()
    : m_startBit(0)
    , m_length(1)
    , m_byteOrder(0)
    , m_isSigned(false)
    , m_factor(1.0)
    , m_offset(0.0)
    , m_min(0.0)
    , m_max(0.0)
    , m_initialValue(0.0)
{
}

double CanSignal::rawToPhysical(int rawValue) const
{
    return rawValue * m_factor + m_offset;
}

int CanSignal::physicalToRaw(double physicalValue) const
{
    return static_cast<int>((physicalValue - m_offset) / m_factor);
}

QString CanSignal::getValueDescription(int rawValue) const
{
    if (m_valueTable.contains(rawValue)) {
        return m_valueTable[rawValue];
    }
    return QString::number(rawValue);
}

QString CanSignal::getReceiversAsString() const
{
    return m_receivers.join(", ");
}
