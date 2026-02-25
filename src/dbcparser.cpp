#include "dbcparser.h"
#include "dbcexcelconverter.h"

#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QTextStream>

namespace {
QRegularExpression makeRegex(const char *pattern)
{
    return QRegularExpression(QString::fromLatin1(pattern));
}

QString normalizeFrameFormat(const QString &format)
{
    if (format.compare("StandardCAN_FD", Qt::CaseInsensitive) == 0) {
        return "CANFD Standard";
    }
    if (format.compare("ExtendedCAN_FD", Qt::CaseInsensitive) == 0) {
        return "CANFD Extended";
    }
    if (format.compare("StandardCAN", Qt::CaseInsensitive) == 0) {
        return "CAN Standard";
    }
    if (format.compare("ExtendedCAN", Qt::CaseInsensitive) == 0) {
        return "CAN Extended";
    }
    return format;
}
}

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
    m_nodes.clear();
    m_version.clear();
    m_busType.clear();
    m_documentTitle.clear();
    m_messageAttributeEnums.clear();
    m_signalAttributeEnums.clear();
}

bool DbcParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open" << filePath;
        return false;
    }

    clear();

    QTextStream in(&file);
    QString line;
    while (in.readLineInto(&line)) {
        if (!parseLine(line.trimmed())) {
            qWarning() << "Failed to parse line:" << line;
        }
    }
    return true;
}

bool DbcParser::loadFromExcelImport(DbcExcelConverter::ImportResult &result)
{
    clear();
    m_version = result.version;
    m_busType = result.busType;
    m_documentTitle = result.documentTitle;
    m_nodes = result.nodes;
    m_messages = result.messages;
    result.messages.clear();
    for (CanMessage *msg : m_messages) {
        m_messageMap[msg->getId()] = msg;
    }
    return true;
}

bool DbcParser::parseLine(const QString &line)
{
    if (line.isEmpty() || line.startsWith("//")) {
        return true;
    }

    if (line.startsWith("VERSION")) {
        QRegularExpression regex = makeRegex("VERSION\\s+\"([^\"]*)\"");
        const QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch()) {
            m_version = match.captured(1);
        }
        return true;
    }

    if (line.startsWith("BU_:")) {
        const QString nodesPart = line.section(':', 1).trimmed();
        const QStringList nodes = nodesPart.split(QRegularExpression(QStringLiteral("\\s+")), QString::SkipEmptyParts);
        for (const QString &node : nodes) {
            if (!m_nodes.contains(node)) {
                m_nodes.append(node);
            }
        }
        return true;
    }

    if (line.startsWith("BO_TX_BU_")) {
        return parseBoTxBu(line);
    }

    if (line.startsWith("CM_")) {
        return parseComment(line);
    }

    if (line.startsWith("BA_DEF_")) {
        return parseAttributeDefinition(line);
    }

    if (line.startsWith("BA_DEF_DEF_")) {
        return true;
    }

    if (line.startsWith("BA_")) {
        return parseAttribute(line);
    }

    if (line.startsWith("VAL_")) {
        return parseValueTable(line);
    }

    if (line.startsWith("BO_")) {
        return parseMessage(line);
    }

    if (line.startsWith("SG_")) {
        return parseSignal(line);
    }

    return true;
}

bool DbcParser::parseMessage(const QString &line)
{
    QRegularExpression regex = makeRegex("BO_\\s+(\\d+)\\s+([^:]+):\\s+(\\d+)\\s+([^\\s]+)");
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    auto *message = new CanMessage();
    message->setId(match.captured(1).toInt());
    message->setName(match.captured(2).trimmed());
    message->setLength(match.captured(3).toInt());
    message->setTransmitter(match.captured(4));

    m_messages.append(message);
    m_messageMap[message->getId()] = message;
    return true;
}

bool DbcParser::parseSignal(const QString &line)
{
    QRegularExpression regex = makeRegex(
        "SG_\\s+([^\\s:]+)\\s*:\\s*(\\d+)\\|(\\d+)@(\\d+)([+-])\\s*\\(([^,]+),([^)]+)\\)\\s*\\[([^|]+)\\|([^\\]]+)\\]\\s*\"([^\"]*)\"\\s*(.*)");
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch() || m_messages.isEmpty()) {
        return false;
    }

    auto *signal = new CanSignal();
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

    QString receiversStr = match.captured(11).trimmed();
    receiversStr.remove(';');
    const QStringList receivers = receiversStr.split(QRegularExpression(QStringLiteral("[\\s,]+")), QString::SkipEmptyParts);
    signal->setReceivers(receivers);

    m_messages.last()->addSignal(signal);
    return true;
}

bool DbcParser::parseValueTable(const QString &line)
{
    QRegularExpression regex = makeRegex("VAL_\\s+(\\d+)\\s+([^\\s]+)\\s+(.+);");
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    CanMessage *message = getMessage(match.captured(1).toInt());
    if (!message) {
        return false;
    }

    CanSignal *signal = message->getSignal(match.captured(2));
    if (!signal) {
        return false;
    }

    QMap<int, QString> valueTable;
    QRegularExpression valueRegex = makeRegex("(-?\\d+)\\s+\"([^\"]+)\"");
    QRegularExpressionMatchIterator it = valueRegex.globalMatch(match.captured(3));
    while (it.hasNext()) {
        const QRegularExpressionMatch valueMatch = it.next();
        valueTable[valueMatch.captured(1).toInt()] = valueMatch.captured(2);
    }
    signal->setValueTable(valueTable);
    return true;
}

bool DbcParser::parseAttribute(const QString &line)
{
    QRegularExpression docTitleRegex = makeRegex("BA_\\s+\"DocumentTitle\"\\s+\"([^\"]*)\"");
    const QRegularExpressionMatch docTitleMatch = docTitleRegex.match(line);
    if (docTitleMatch.hasMatch()) {
        m_documentTitle = docTitleMatch.captured(1);
        return true;
    }

    QRegularExpression busTypeRegex = makeRegex("BA_\\s+\"BusType\"\\s+\"([^\"]+)\"");
    const QRegularExpressionMatch busMatch = busTypeRegex.match(line);
    if (busMatch.hasMatch()) {
        m_busType = busMatch.captured(1);
        return true;
    }

    QRegularExpression msgRegex = makeRegex("BA_\\s+\"([^\"]+)\"\\s+BO_\\s+(\\d+)\\s+([^;]+);");
    const QRegularExpressionMatch msgMatch = msgRegex.match(line);
    if (msgMatch.hasMatch()) {
        const QString attrName = msgMatch.captured(1);
        CanMessage *message = getMessage(msgMatch.captured(2).toInt());
        if (!message) {
            return true;
        }

        QString valuePart = msgMatch.captured(3).trimmed();
        if (valuePart.startsWith('"') && valuePart.endsWith('"')) {
            valuePart = valuePart.mid(1, valuePart.size() - 2);
        }

        if (attrName == "GenMsgCycleTime") {
            message->setCycleTime(valuePart.toInt());
        } else if (attrName == "GenMsgSendType") {
            const QString mapped = enumValueLookup(m_messageAttributeEnums, attrName, valuePart.toInt());
            message->setSendType(mapped.isEmpty() ? valuePart : mapped);
        } else if (attrName == "VFrameFormat") {
            const QString mapped = enumValueLookup(m_messageAttributeEnums, attrName, valuePart.toInt());
            message->setFrameFormat(mapped.isEmpty() ? valuePart : mapped);
            message->setMessageType(normalizeFrameFormat(message->getFrameFormat()));
        } else if (attrName == "GenMsgNrOfRepetitions" || attrName == "GenMsgNrOfRepetition") {
            message->setNrOfRepetitions(valuePart.toInt());
        } else if (attrName == "GenMsgDelayTime") {
            message->setDelayTime(valuePart.toInt());
        } else if (attrName == "GenMsgCycleTimeFast") {
            message->setCycleTimeFast(valuePart.toInt());
        }
        return true;
    }

    QRegularExpression sigRegex = makeRegex("BA_\\s+\"([^\"]+)\"\\s+SG_\\s+(\\d+)\\s+([^\\s]+)\\s+([^;]+);");
    const QRegularExpressionMatch sigMatch = sigRegex.match(line);
    if (sigMatch.hasMatch()) {
        const QString attrName = sigMatch.captured(1);
        CanMessage *message = getMessage(sigMatch.captured(2).toInt());
        if (!message) {
            return true;
        }

        CanSignal *signal = message->getSignal(sigMatch.captured(3));
        if (!signal) {
            return true;
        }

        QString valuePart = sigMatch.captured(4).trimmed();
        if (valuePart.startsWith('"') && valuePart.endsWith('"')) {
            valuePart = valuePart.mid(1, valuePart.size() - 2);
        }

        if (attrName == "GenSigSendType") {
            const QString mapped = enumValueLookup(m_signalAttributeEnums, attrName, valuePart.toInt());
            signal->setSendType(mapped.isEmpty() ? valuePart : mapped);
        } else if (attrName == "GenSigStartValue") {
            signal->setInitialValue(valuePart.toDouble());
        } else if (attrName == "GenSigSNA") {
            signal->setInactiveValueHex(valuePart);
        }
        return true;
    }

    return true;
}

bool DbcParser::parseAttributeDefinition(const QString &line)
{
    QRegularExpression enumRegex = makeRegex("BA_DEF_\\s+(BO_|SG_)\\s+\"([^\"]+)\"\\s+ENUM\\s+(.+);");
    const QRegularExpressionMatch match = enumRegex.match(line);
    if (!match.hasMatch()) {
        return true;
    }

    const QString scope = match.captured(1);
    const QString attrName = match.captured(2);
    const QString valuesPart = match.captured(3);

    QStringList values;
    QRegularExpression valueRegex = makeRegex("\"([^\"]*)\"");
    QRegularExpressionMatchIterator it = valueRegex.globalMatch(valuesPart);
    while (it.hasNext()) {
        values.append(it.next().captured(1));
    }

    if (scope == "BO_") {
        m_messageAttributeEnums[attrName] = values;
    } else if (scope == "SG_") {
        m_signalAttributeEnums[attrName] = values;
    }
    return true;
}

bool DbcParser::parseComment(const QString &line)
{
    QRegularExpression msgRegex = makeRegex("CM_\\s+BO_\\s+(\\d+)\\s+\"([^\"]*)\";");
    const QRegularExpressionMatch msgMatch = msgRegex.match(line);
    if (msgMatch.hasMatch()) {
        CanMessage *message = getMessage(msgMatch.captured(1).toInt());
        if (message) {
            message->setComment(msgMatch.captured(2));
        }
        return true;
    }

    QRegularExpression sigRegex = makeRegex("CM_\\s+SG_\\s+(\\d+)\\s+([^\\s]+)\\s+\"([^\"]*)\";");
    const QRegularExpressionMatch sigMatch = sigRegex.match(line);
    if (sigMatch.hasMatch()) {
        CanMessage *message = getMessage(sigMatch.captured(1).toInt());
        if (!message) {
            return true;
        }
        CanSignal *signal = message->getSignal(sigMatch.captured(2));
        if (signal) {
            signal->setDescription(sigMatch.captured(3));
        }
        return true;
    }
    return true;
}

bool DbcParser::parseBoTxBu(const QString &line)
{
    QRegularExpression regex = makeRegex("BO_TX_BU_\\s+(\\d+)\\s*:\\s*([^;]*);?");
    const QRegularExpressionMatch match = regex.match(line);
    if (!match.hasMatch()) {
        return false;
    }

    QString receiversStr = match.captured(2).trimmed();
    receiversStr.remove(';');
    QStringList receivers = receiversStr.split(QRegularExpression(QStringLiteral("[\\s,]+")), QString::SkipEmptyParts);

    CanMessage *message = getMessage(match.captured(1).toInt());
    if (message) {
        message->setReceivers(receivers);
    }
    return true;
}

CanMessage *DbcParser::getMessage(int id) const
{
    return m_messageMap.value(id, nullptr);
}

double DbcParser::parseDouble(const QString &str)
{
    bool ok = false;
    const double value = str.toDouble(&ok);
    return ok ? value : 0.0;
}

int DbcParser::parseInt(const QString &str)
{
    bool ok = false;
    const int value = str.toInt(&ok);
    return ok ? value : 0;
}

QString DbcParser::enumValueLookup(const QMap<QString, QStringList> &map, const QString &attrName, int index) const
{
    const QStringList values = map.value(attrName);
    if (index < 0 || index >= values.size()) {
        return QString();
    }
    return values.at(index);
}
