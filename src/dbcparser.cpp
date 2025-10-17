#include "dbcparser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

DbcParser::DbcParser()
{
}

DbcParser::~DbcParser()
{
    clear();
}

void DbcParser::clear()
{
    for (CanMessage *message : m_messages) {
        for (CanSignal *signal : message->getSignals()) {
            delete signal;
        }
        delete message;
    }
    m_messages.clear();
    m_messageMap.clear();
    m_version.clear();
    m_busType.clear();
}

bool DbcParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << filePath;
        return false;
    }
    
    clear();
    
    QTextStream in(&file);
    QString line;
    
    while (in.readLineInto(&line)) {
        if (!parseLine(line.trimmed())) {
            qDebug() << "Failed to parse line:" << line;
        }
    }
    
    file.close();
    return true;
}

bool DbcParser::parseLine(const QString &line)
{
    if (line.isEmpty() || line.startsWith("//")) {
        return true; // Skip empty lines and comments
    }
    
    if (line.startsWith("VERSION")) {
        QRegularExpression regex("VERSION\\s+\"([^\"]+)\"");
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch()) {
            m_version = match.captured(1);
        }
        return true;
    }
    
    if (line.startsWith("BA_ \"BusType\"")) {
        QRegularExpression regex("BA_\\s+\"BusType\"\\s+\"([^\"]+)\"");
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch()) {
            m_busType = match.captured(1);
        }
        return true;
    }
    
    if (line.startsWith("BO_")) {
        return parseMessage(line);
    }
    
    if (line.startsWith("SG_")) {
        return parseSignal(line, nullptr); // Will be handled by context
    }
    
    if (line.startsWith("VAL_")) {
        return parseValueTable(line);
    }
    
    if (line.startsWith("BA_")) {
        return parseAttribute(line);
    }
    
    return true; // Skip unknown lines
}

bool DbcParser::parseMessage(const QString &line)
{
    // Format: BO_ ID Name: Length Transmitter
    QRegularExpression regex(R"(BO_\s+(\d+)\s+([^:]+):\s+(\d+)\s+(\w+))");
    QRegularExpressionMatch match = regex.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    CanMessage *message = new CanMessage();
    message->setId(match.captured(1).toInt());
    message->setName(match.captured(2).trimmed());
    message->setLength(match.captured(3).toInt());
    message->setTransmitter(match.captured(4));
    
    m_messages.append(message);
    m_messageMap[message->getId()] = message;
    
    return true;
}

bool DbcParser::parseSignal(const QString &line, CanMessage *message)
{
    // Format: SG_ Name : StartBit|Length@ByteOrder+/- (Factor,Offset) [Min|Max] "Unit" Receiver
    QRegularExpression regex("SG_\\s+([^:]+)\\s*:\\s*(\\d+)\\|(\\d+)@(\\d+)([+-])\\s*\\(([^,]+),([^)]+)\\)\\s*\\[([^|]+)\\|([^\\]]+)\\]\\s*\"([^\"]*)\"\\s*(\\w+)");
    QRegularExpressionMatch match = regex.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    CanSignal *signal = new CanSignal();
    signal->setName(match.captured(1).trimmed());
    signal->setStartBit(match.captured(2).toInt());
    signal->setLength(match.captured(3).toInt());
    signal->setByteOrder(match.captured(4).toInt());
    signal->setSigned(match.captured(5) == "-");
    signal->setFactor(parseDouble(match.captured(6)));
    signal->setOffset(parseDouble(match.captured(7)));
    signal->setMin(parseDouble(match.captured(8)));
    signal->setMax(parseDouble(match.captured(9)));
    signal->setUnit(match.captured(10));
    signal->setReceiver(match.captured(11));
    
    // Add to the last message
    if (!m_messages.isEmpty()) {
        m_messages.last()->addSignal(signal);
    }
    
    return true;
}

bool DbcParser::parseValueTable(const QString &line)
{
    // Format: VAL_ MessageId SignalName Value1 "Description1" Value2 "Description2" ... ;
    QRegularExpression regex(R"(VAL_\s+(\d+)\s+(\w+)\s+(.+);)");
    QRegularExpressionMatch match = regex.match(line);
    
    if (!match.hasMatch()) {
        return false;
    }
    
    int messageId = match.captured(1).toInt();
    QString signalName = match.captured(2);
    QString valuesStr = match.captured(3);
    
    CanMessage *message = getMessage(messageId);
    if (!message) {
        return false;
    }
    
    CanSignal *signal = message->getSignal(signalName);
    if (!signal) {
        return false;
    }
    
    // Parse value descriptions
    QMap<int, QString> valueTable;
    QRegularExpression valueRegex("(\\d+)\\s+\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = valueRegex.globalMatch(valuesStr);
    
    while (it.hasNext()) {
        QRegularExpressionMatch valueMatch = it.next();
        int value = valueMatch.captured(1).toInt();
        QString description = valueMatch.captured(2);
        valueTable[value] = description;
    }
    
    signal->setValueTable(valueTable);
    return true;
}

bool DbcParser::parseAttribute(const QString &line)
{
    // Parse cycle time attributes
    QRegularExpression cycleRegex(R"(BA_\s+"GenMsgCycleTime"\s+BO_\s+(\d+)\s+(\d+))");
    QRegularExpressionMatch cycleMatch = cycleRegex.match(line);
    
    if (cycleMatch.hasMatch()) {
        int messageId = cycleMatch.captured(1).toInt();
        int cycleTime = cycleMatch.captured(2).toInt();
        
        CanMessage *message = getMessage(messageId);
        if (message) {
            message->setCycleTime(cycleTime);
        }
        return true;
    }
    
    // Parse frame format attributes
    QRegularExpression frameRegex(R"(BA_\s+"VFrameFormat"\s+BO_\s+(\d+)\s+(\d+))");
    QRegularExpressionMatch frameMatch = frameRegex.match(line);
    
    if (frameMatch.hasMatch()) {
        int messageId = frameMatch.captured(1).toInt();
        int frameFormat = frameMatch.captured(2).toInt();
        
        CanMessage *message = getMessage(messageId);
        if (message) {
            QString formatStr;
            switch (frameFormat) {
                case 0: formatStr = "StandardCAN"; break;
                case 1: formatStr = "ExtendedCAN"; break;
                case 14: formatStr = "StandardCAN_FD"; break;
                case 15: formatStr = "ExtendedCAN_FD"; break;
                default: formatStr = "Unknown"; break;
            }
            message->setFrameFormat(formatStr);
        }
        return true;
    }
    
    return true; // Skip other attributes
}

CanMessage* DbcParser::getMessage(int id) const
{
    return m_messageMap.value(id, nullptr);
}

double DbcParser::parseDouble(const QString &str)
{
    bool ok;
    double value = str.toDouble(&ok);
    return ok ? value : 0.0;
}

int DbcParser::parseInt(const QString &str)
{
    bool ok;
    int value = str.toInt(&ok);
    return ok ? value : 0;
}
