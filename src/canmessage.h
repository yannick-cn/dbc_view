#ifndef CANMESSAGE_H
#define CANMESSAGE_H

#include <QString>
#include <QList>
#include <QMap>
#include "cansignal.h"

class CanMessage
{
public:
    CanMessage();
    
    // Getters
    int getId() const { return m_id; }
    QString getName() const { return m_name; }
    int getLength() const { return m_length; }
    QString getTransmitter() const { return m_transmitter; }
    QList<CanSignal*> getSignals() const { return m_signals; }
    CanSignal* getSignal(const QString &name) const;
    int getCycleTime() const { return m_cycleTime; }
    QString getFrameFormat() const { return m_frameFormat; }
    
    // Setters
    void setId(int id) { m_id = id; }
    void setName(const QString &name) { m_name = name; }
    void setLength(int length) { m_length = length; }
    void setTransmitter(const QString &transmitter) { m_transmitter = transmitter; }
    void addSignal(CanSignal *signal);
    void setCycleTime(int cycleTime) { m_cycleTime = cycleTime; }
    void setFrameFormat(const QString &frameFormat) { m_frameFormat = frameFormat; }
    
    // Utility functions
    QString getFormattedId() const;
    QString getFormattedLength() const;

private:
    int m_id;
    QString m_name;
    int m_length;
    QString m_transmitter;
    QList<CanSignal*> m_signals;
    int m_cycleTime; // in ms
    QString m_frameFormat;
};

#endif // CANMESSAGE_H
