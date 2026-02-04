#include "dbcwriter.h"

#include <QFile>
#include <QLocale>
#include <QTextStream>
#include <QtGlobal>

#include <cmath>
#include <limits>

namespace {
const QStringList kMessageSendTypes = {
    "Cycle",
    "OnChange",
    "OnWrite",
    "OnWriteWithRepetition",
    "OnChangeWithRepetition",
    "IfActive",
    "IfActiveWithRepetition",
    "NoMsgSendType"
};

const QStringList kSignalSendTypes = {
    "Cycle",
    "OnWrite",
    "OnWriteWithRepetition",
    "OnChange",
    "OnChangeWithRepetition",
    "IfActive",
    "IfActiveWithRepetition",
    "NoSigSendType",
    "vector_leerstring"
};

const QStringList kFrameFormats = {
    "StandardCAN",
    "ExtendedCAN",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "reserved",
    "StandardCAN_FD",
    "ExtendedCAN_FD"
};

QString escape(const QString &text)
{
    QString escaped = text;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    return escaped;
}

QString formatDouble(double value)
{
    const double absVal = std::abs(value);
    if (absVal >= 1e10 || (absVal > 0 && absVal < 1e-6)) {
        QString s = QLocale::c().toString(value, 'e', 15);
        s.replace('e', 'E');
        if (s.contains('E')) {
            int expPos = s.indexOf('E');
            QString expPart = s.mid(expPos + 1);
            bool ok = false;
            int exp = expPart.toInt(&ok);
            if (ok) {
                QString expStr = QString::number(std::abs(exp));
                if (expStr.length() < 3) {
                    expStr = QString(3 - expStr.length(), '0') + expStr;
                }
                s = s.left(expPos + 1) + (exp >= 0 ? "+" : "-") + expStr;
            }
        }
        return s;
    }
    return QLocale::c().toString(value, 'g', 15);
}

QString formatInt(quint64 value)
{
    return QString::number(value);
}

int messageSendTypeIndex(const QString &sendType)
{
    const int idx = kMessageSendTypes.indexOf(sendType, Qt::CaseInsensitive);
    return idx >= 0 ? idx : 0;
}

int signalSendTypeIndex(const QString &sendType)
{
    const int idx = kSignalSendTypes.indexOf(sendType, Qt::CaseInsensitive);
    return idx >= 0 ? idx : kSignalSendTypes.size() - 1;
}

int frameFormatIndex(const QString &frameFormat)
{
    const int idx = kFrameFormats.indexOf(frameFormat, Qt::CaseInsensitive);
    if (idx >= 0) {
        return idx;
    }
    if (frameFormat.contains("StandardCAN_FD", Qt::CaseInsensitive)) {
        return kFrameFormats.indexOf("StandardCAN_FD");
    }
    if (frameFormat.contains("ExtendedCAN_FD", Qt::CaseInsensitive)) {
        return kFrameFormats.indexOf("ExtendedCAN_FD");
    }
    if (frameFormat.contains("ExtendedCAN", Qt::CaseInsensitive)) {
        return kFrameFormats.indexOf("ExtendedCAN");
    }
    return kFrameFormats.indexOf("StandardCAN");
}

QString canonicalFrameFormat(const CanMessage *message)
{
    const QString frameFormat = message->getFrameFormat();
    if (!frameFormat.isEmpty()) {
        return frameFormat;
    }

    const QString type = message->getMessageType();
    if (type.contains("CANFD", Qt::CaseInsensitive)) {
        return type.contains("Extended", Qt::CaseInsensitive) ? "ExtendedCAN_FD" : "StandardCAN_FD";
    }
    if (type.contains("Extended", Qt::CaseInsensitive)) {
        return "ExtendedCAN";
    }
    return "StandardCAN";
}

QString fallbackNode(const QStringList &nodes)
{
    return nodes.isEmpty() ? QStringLiteral("Vector__XXX") : nodes.first();
}

QString joinReceivers(const QStringList &receivers, const QString &fallback)
{
    if (!receivers.isEmpty()) {
        return receivers.join(',');
    }
    return fallback;
}

QStringList ensureNode(const QStringList &nodes, const QString &node)
{
    QStringList result = nodes;
    if (!node.isEmpty() && !result.contains(node)) {
        result.append(node);
    }
    return result;
}
} // namespace

bool DbcWriter::write(const QString &filePath,
                      const QString &version,
                      const QString &busType,
                      const QStringList &nodes,
                      const QList<CanMessage*> &messages,
                      QString *error)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        if (error) {
            *error = QString("Unable to open %1 for writing").arg(filePath);
        }
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    out << "VERSION \"" << escape(version) << "\"\n\n\n";

    out << "NS_ :\n";
    out << "\tNS_DESC_\n\tCM_\n\tBA_DEF_\n\tBA_\n\tVAL_\n\tCAT_DEF_\n\tCAT_\n\tFILTER\n";
    out << "\tBA_DEF_DEF_\n\tEV_DATA_\n\tENVVAR_DATA_\n\tSGTYPE_\n\tSGTYPE_VAL_\n";
    out << "\tBA_DEF_SGTYPE_\n\tBA_SGTYPE_\n\tSIG_TYPE_REF_\n\tVAL_TABLE_\n";
    out << "\tSIG_GROUP_\n\tSIG_VALTYPE_\n\tSIGTYPE_VALTYPE_\n\tBO_TX_BU_\n";
    out << "\tBA_DEF_REL_\n\tBA_REL_\n\tBA_DEF_DEF_REL_\n\tBU_SG_REL_\n";
    out << "\tBU_EV_REL_\n\tBU_BO_REL_\n\tSG_MUL_VAL_\n\n";

    out << "BS_:\n\n";

    QStringList nodeList = nodes;
    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        nodeList = ensureNode(nodeList, message->getTransmitter());
        for (const QString &receiver : message->getReceivers()) {
            nodeList = ensureNode(nodeList, receiver);
        }
        for (CanSignal *signal : message->getSignals()) {
            if (!signal) continue;
            for (const QString &receiver : signal->getReceivers()) {
                nodeList = ensureNode(nodeList, receiver);
            }
        }
    }
    nodeList.removeDuplicates();

    out << "BU_:";
    QStringList buNodes;
    for (const QString &node : nodeList) {
        if (node != QLatin1String("Vector__XXX")) {
            buNodes.append(node);
        }
    }
    if (buNodes.isEmpty()) {
        out << " Vector__XXX\n\n";
    } else {
        for (const QString &node : buNodes) {
            out << ' ' << node;
        }
        out << "\n\n";
    }

    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        const QString transmitter = message->getTransmitter().isEmpty()
                                        ? fallbackNode(nodeList)
                                        : message->getTransmitter();
        out << "\nBO_ " << message->getId() << ' ' << message->getName() << ": "
            << message->getLength() << ' ' << transmitter << "\n";

        const QStringList msgReceivers = message->getReceivers();
        const QString receiverListForSignal = msgReceivers.isEmpty()
            ? transmitter
            : joinReceivers(msgReceivers, QString());

        for (CanSignal *signal : message->getSignals()) {
            if (!signal) {
                continue;
            }
            const QString sign = signal->isSigned() ? "-" : "+";
            const QString receivers =
                joinReceivers(signal->getReceivers(), receiverListForSignal);

            out << " SG_ " << signal->getName() << " : "
                << signal->getStartBit() << '|' << signal->getLength()
                << '@' << signal->getByteOrder()
                << sign << " ("
                << formatDouble(signal->getFactor()) << ','
                << formatDouble(signal->getOffset()) << ") ["
                << formatDouble(signal->getMin()) << '|'
                << formatDouble(signal->getMax()) << "] \""
                << escape(signal->getUnit()) << "\" "
                << receivers << "\n";
        }
    }

    for (CanMessage *message : messages) {
        if (!message) continue;
        const QStringList msgReceivers = message->getReceivers();
        if (!msgReceivers.isEmpty()) {
            out << "BO_TX_BU_ " << message->getId() << " : " << joinReceivers(msgReceivers, QString()) << ";\n";
        }
    }

    out << '\n';

    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        if (!message->getComment().isEmpty()) {
            out << "CM_ BO_ " << message->getId() << " \"" << escape(message->getComment()) << "\";\n";
        }
        for (CanSignal *signal : message->getSignals()) {
            if (signal && !signal->getDescription().isEmpty()) {
                out << "CM_ SG_ " << message->getId() << ' ' << signal->getName() << " \""
                    << escape(signal->getDescription()) << "\";\n";
            }
        }
    }

    out << '\n';

    out << "BA_DEF_ BO_ \"GenMsgCycleTime\" INT 0 65535;\n";
    out << "BA_DEF_ BO_ \"GenMsgCycleTimeActive\" INT 0 65535;\n";
    out << "BA_DEF_ BO_ \"GenMsgCycleTimeFast\" INT 0 0;\n";
    out << "BA_DEF_ BO_ \"GenMsgDelayTime\" INT 0 65535;\n";
    out << "BA_DEF_ BO_ \"GenMsgNrOfRepetition\" INT 0 0;\n";
    out << "BA_DEF_ BO_ \"NmMessage\" ENUM \"No\",\"Yes\";\n";
    out << "BA_DEF_ BO_ \"DiagRequest\" ENUM \"No\",\"Yes\";\n";
    out << "BA_DEF_ BO_ \"DiagResponse\" ENUM \"No\",\"Yes\";\n";
    out << "BA_DEF_ BO_ \"GenMsgSendType\" ENUM ";
    for (int i = 0; i < kMessageSendTypes.size(); ++i) {
        out << '"' << kMessageSendTypes.at(i) << '"';
        if (i != kMessageSendTypes.size() - 1) out << ',';
    }
    out << ";\n";
    out << "BA_DEF_ BO_ \"VFrameFormat\" ENUM ";
    for (int i = 0; i < kFrameFormats.size(); ++i) {
        out << '"' << kFrameFormats.at(i) << '"';
        if (i != kFrameFormats.size() - 1) out << ',';
    }
    out << ";\n";
    out << "BA_DEF_ SG_ \"GenSigStartDelayTime\" INT 0 100000;\n";
    out << "BA_DEF_ SG_ \"GenSigILSupport\" ENUM \"No\",\"Yes\";\n";
    out << "BA_DEF_ SG_ \"GenSigInactiveValue\" HEX 0 0;\n";
    out << "BA_DEF_ SG_ \"GenSigInvalidValue\" HEX 0 0;\n";
    out << "BA_DEF_ SG_ \"GenSigSNA\" STRING ;\n";
    out << "BA_DEF_ SG_ \"GenSigSendType\" ENUM ";
    for (int i = 0; i < kSignalSendTypes.size(); ++i) {
        out << '"' << kSignalSendTypes.at(i) << '"';
        if (i != kSignalSendTypes.size() - 1) out << ',';
    }
    out << ";\n";
    out << "BA_DEF_ SG_ \"GenSigStartValue\" FLOAT 0 100000000000;\n";
    out << "BA_DEF_ \"BusType\" STRING ;\n";
    out << "BA_DEF_ \"ProtocolType\" STRING ;\n";
    out << "BA_DEF_ \"Manufacturer\" STRING ;\n";
    out << "BA_DEF_ \"DBName\" STRING ;\n";
    out << "BA_DEF_ \"Baudrate\" INT 0 1000000;\n";
    out << "BA_DEF_ \"NmType\" STRING ;\n";
    out << "BA_DEF_ \"VersionYear\" INT 2010 2999;\n";
    out << "BA_DEF_ \"NmMessageCount\" INT 0 255;\n";
    out << "BA_DEF_ BU_ \"NodeLayerModules\" STRING ;\n";

    out << "BA_DEF_DEF_ \"GenMsgCycleTime\" 0;\n";
    out << "BA_DEF_DEF_ \"GenMsgCycleTimeActive\" 0;\n";
    out << "BA_DEF_DEF_ \"GenMsgCycleTimeFast\" 0;\n";
    out << "BA_DEF_DEF_ \"GenMsgDelayTime\" 0;\n";
    out << "BA_DEF_DEF_ \"GenMsgNrOfRepetition\" 0;\n";
    out << "BA_DEF_DEF_ \"GenMsgSendType\" \"Cycle\";\n";
    out << "BA_DEF_DEF_ \"VFrameFormat\" \"StandardCAN\";\n";
    out << "BA_DEF_DEF_ \"GenSigStartDelayTime\" 0;\n";
    out << "BA_DEF_DEF_ \"GenSigILSupport\" \"Yes\";\n";
    out << "BA_DEF_DEF_ \"GenSigSNA\" \"\";\n";
    out << "BA_DEF_DEF_ \"GenSigSendType\" \"NoSigSendType\";\n";
    out << "BA_DEF_DEF_ \"GenSigStartValue\" 0;\n";
    out << "BA_DEF_DEF_ \"GenSigInactiveValue\" 0;\n";
    out << "BA_DEF_DEF_ \"GenSigInvalidValue\" 0;\n";
    out << "BA_DEF_DEF_ \"BusType\" \"\";\n";
    out << "BA_DEF_DEF_ \"ProtocolType\" \"CAN\";\n";
    out << "BA_DEF_DEF_ \"Manufacturer\" \"\";\n";
    out << "BA_DEF_DEF_ \"DBName\" \"\";\n";
    out << "BA_DEF_DEF_ \"Baudrate\" 500000;\n";
    out << "BA_DEF_DEF_ \"NmType\" \"OSEK\";\n";
    out << "BA_DEF_DEF_ \"VersionYear\" 2019;\n";
    out << "BA_DEF_DEF_ \"NmMessageCount\" 128;\n";
    out << "BA_DEF_DEF_ \"NodeLayerModules\" \"\";\n\n";

    out << "BA_ \"BusType\" \"" << escape(busType.isEmpty() ? "CAN" : busType) << "\";\n";
    out << "BA_ \"ProtocolType\" \"CAN FD\";\n";
    out << "BA_ \"Manufacturer\" \"JX\";\n";
    out << "BA_ \"DBName\" \"ADCANFD\";\n";
    out << "BA_ \"Baudrate\" 500000;\n";
    out << "BA_ \"NmType\" \"AUTOSAR\";\n";
    out << "BA_ \"NmMessageCount\" 255;\n";
    for (const QString &node : buNodes) {
        out << "BA_ \"NodeLayerModules\" BU_ " << node << " \"CANoeILNLVector.dll\";\n";
    }

    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        if (message->getCycleTime() > 0) {
            out << "BA_ \"GenMsgCycleTime\" BO_ " << message->getId() << ' '
                << message->getCycleTime() << ";\n";
        }
        if (message->getCycleTimeFast() > 0) {
            out << "BA_ \"GenMsgCycleTimeFast\" BO_ " << message->getId() << ' '
                << message->getCycleTimeFast() << ";\n";
        }
        if (message->getNrOfRepetitions() > 0) {
            out << "BA_ \"GenMsgNrOfRepetition\" BO_ " << message->getId() << ' '
                << message->getNrOfRepetitions() << ";\n";
        }
        if (message->getDelayTime() > 0) {
            out << "BA_ \"GenMsgDelayTime\" BO_ " << message->getId() << ' '
                << message->getDelayTime() << ";\n";
        }

        const QString frameFormat = canonicalFrameFormat(message);
        out << "BA_ \"VFrameFormat\" BO_ " << message->getId() << ' '
            << frameFormatIndex(frameFormat) << ";\n";

        out << "BA_ \"GenMsgSendType\" BO_ " << message->getId() << ' '
            << messageSendTypeIndex(message->getSendType()) << ";\n";

        const int msgId = message->getId();
        if (msgId == 1186 || msgId == 1187 || msgId == 1188 || msgId == 1152 || msgId == 1189 || msgId == 1190) {
            out << "BA_ \"NmMessage\" BO_ " << msgId << " 1;\n";
        }
        if (msgId == 1842) {
            out << "BA_ \"DiagRequest\" BO_ " << msgId << " 1;\n";
        }
        if (msgId == 1850) {
            out << "BA_ \"DiagResponse\" BO_ " << msgId << " 1;\n";
        }
    }

    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        for (CanSignal *signal : message->getSignals()) {
            if (!signal) {
                continue;
            }
            out << "BA_ \"GenSigSendType\" SG_ " << message->getId() << ' ' << signal->getName() << ' '
                << signalSendTypeIndex(signal->getSendType()) << ";\n";

            out << "BA_ \"GenSigStartValue\" SG_ " << message->getId() << ' ' << signal->getName() << ' '
                << formatDouble(signal->getInitialValue()) << ";\n";

            out << "BA_ \"GenSigInactiveValue\" SG_ " << message->getId() << ' ' << signal->getName() << " 0;\n";

            if (!signal->getInactiveValueHex().isEmpty()) {
                out << "BA_ \"GenSigSNA\" SG_ " << message->getId() << ' ' << signal->getName() << " \""
                    << escape(signal->getInactiveValueHex()) << "\";\n";
            }
        }
    }

    for (CanMessage *message : messages) {
        if (!message) continue;
        for (CanSignal *signal : message->getSignals()) {
            if (!signal) continue;
            if (!signal->getValueTable().isEmpty()) {
                out << "VAL_ " << message->getId() << ' ' << signal->getName();
                for (auto it = signal->getValueTable().cbegin(); it != signal->getValueTable().cend(); ++it) {
                    out << ' ' << it.key() << " \"" << escape(it.value()) << '"';
                }
                out << ";\n";
            }
        }
    }

    out.flush();
    file.close();
    return true;
}
