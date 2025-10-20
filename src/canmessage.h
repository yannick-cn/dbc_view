#ifndef CANMESSAGE_H
#define CANMESSAGE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QStringList>
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
    QString getSendType() const { return m_sendType; }
    int getCycleTimeFast() const { return m_cycleTimeFast; }
    int getNrOfRepetitions() const { return m_nrOfRepetitions; }
    int getDelayTime() const { return m_delayTime; }
    QString getComment() const { return m_comment; }
    QString getMessageType() const { return m_messageType; }
    QStringList getReceivers() const { return m_receivers; }
    
    // Setters
    void setId(int id) { m_id = id; }
    void setName(const QString &name) { m_name = name; }
    void setLength(int length) { m_length = length; }
    void setTransmitter(const QString &transmitter) { m_transmitter = transmitter; }
    void addSignal(CanSignal *signal);
    void setCycleTime(int cycleTime) { m_cycleTime = cycleTime; }
    void setFrameFormat(const QString &frameFormat) { m_frameFormat = frameFormat; }
    void setSendType(const QString &sendType) { m_sendType = sendType; }
    void setCycleTimeFast(int value) { m_cycleTimeFast = value; }
    void setNrOfRepetitions(int value) { m_nrOfRepetitions = value; }
    void setDelayTime(int value) { m_delayTime = value; }
    void setComment(const QString &comment) { m_comment = comment; }
    void setMessageType(const QString &type) { m_messageType = type; }
    void setReceivers(const QStringList &receivers) { m_receivers = receivers; }
    
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
    QString m_sendType;
    int m_cycleTimeFast;
    int m_nrOfRepetitions;
    int m_delayTime;
    QString m_comment;
    QString m_messageType;
    QStringList m_receivers;
};

#endif // CANMESSAGE_H
