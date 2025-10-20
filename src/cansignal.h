#ifndef CANSIGNAL_H
#define CANSIGNAL_H

#include <QString>
#include <QStringList>
#include <QMap>

class CanSignal
{
public:
    CanSignal();
    
    // Getters
    QString getName() const { return m_name; }
    int getStartBit() const { return m_startBit; }
    int getLength() const { return m_length; }
    int getByteOrder() const { return m_byteOrder; }
    bool isSigned() const { return m_isSigned; }
    double getFactor() const { return m_factor; }
    double getOffset() const { return m_offset; }
    double getMin() const { return m_min; }
    double getMax() const { return m_max; }
    QString getUnit() const { return m_unit; }
    QStringList getReceivers() const { return m_receivers; }
    QMap<int, QString> getValueTable() const { return m_valueTable; }
    QString getDescription() const { return m_description; }
    QString getSendType() const { return m_sendType; }
    double getInitialValue() const { return m_initialValue; }
    QString getInvalidValueHex() const { return m_invalidValueHex; }
    QString getInactiveValueHex() const { return m_inactiveValueHex; }
    QString getReceiversAsString() const;
    
    // Setters
    void setName(const QString &name) { m_name = name; }
    void setStartBit(int startBit) { m_startBit = startBit; }
    void setLength(int length) { m_length = length; }
    void setByteOrder(int byteOrder) { m_byteOrder = byteOrder; }
    void setSigned(bool isSigned) { m_isSigned = isSigned; }
    void setFactor(double factor) { m_factor = factor; }
    void setOffset(double offset) { m_offset = offset; }
    void setMin(double min) { m_min = min; }
    void setMax(double max) { m_max = max; }
    void setUnit(const QString &unit) { m_unit = unit; }
    void setReceivers(const QStringList &receivers) { m_receivers = receivers; }
    void setValueTable(const QMap<int, QString> &valueTable) { m_valueTable = valueTable; }
    void setDescription(const QString &description) { m_description = description; }
    void setSendType(const QString &sendType) { m_sendType = sendType; }
    void setInitialValue(double initialValue) { m_initialValue = initialValue; }
    void setInvalidValueHex(const QString &value) { m_invalidValueHex = value; }
    void setInactiveValueHex(const QString &value) { m_inactiveValueHex = value; }
    
    // Utility functions
    double rawToPhysical(int rawValue) const;
    int physicalToRaw(double physicalValue) const;
    QString getValueDescription(int rawValue) const;

private:
    QString m_name;
    int m_startBit;
    int m_length;
    int m_byteOrder; // 0 = little endian, 1 = big endian
    bool m_isSigned;
    double m_factor;
    double m_offset;
    double m_min;
    double m_max;
    QString m_unit;
    QStringList m_receivers;
    QString m_description;
    QString m_sendType;
    double m_initialValue;
    QString m_invalidValueHex;
    QString m_inactiveValueHex;
    QMap<int, QString> m_valueTable; // Raw value -> Description mapping
};

#endif // CANSIGNAL_H
