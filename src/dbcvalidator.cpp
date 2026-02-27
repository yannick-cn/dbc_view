#include "dbcvalidator.h"
#include "canmessage.h"
#include "cansignal.h"

#include <QtGlobal>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace
{

qint64 parseHexToSigned(const QString &text, bool *ok)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }

    bool localOk = false;
    qint64 value = 0;
    if (trimmed.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = trimmed.mid(2).toLongLong(&localOk, 16);
    } else {
        value = trimmed.toLongLong(&localOk, 10);
    }
    if (ok) {
        *ok = localOk;
    }
    return value;
}

void addError(ValidationResult &result, const QString &msgName, const QString &sigName, const QString &text)
{
    if (sigName.isEmpty()) {
        result.errors.append(QString("[%1] %2").arg(msgName, text));
    } else {
        result.errors.append(QString("[%1 / %2] %3").arg(msgName, sigName, text));
    }
    result.ok = false;
}

qint64 rawMinSigned(int length)
{
    if (length <= 0 || length > 64) {
        return 0;
    }
    if (length >= 64) {
        return std::numeric_limits<qint64>::min();
    }
    return -(1LL << (length - 1));
}

qint64 rawMaxSigned(int length)
{
    if (length <= 0 || length > 64) {
        return 0;
    }
    if (length >= 64) {
        return std::numeric_limits<qint64>::max();
    }
    return (1LL << (length - 1)) - 1;
}

quint64 rawMaxUnsigned(int length)
{
    if (length <= 0) {
        return 0;
    }
    if (length >= 64) {
        return std::numeric_limits<quint64>::max();
    }
    return (1ULL << length) - 1;
}

bool rawInSignedRange(qint64 raw, int length)
{
    return length > 0 && length <= 64 && raw >= rawMinSigned(length) && raw <= rawMaxSigned(length);
}

bool rawInUnsignedRange(quint64 raw, int length)
{
    return length > 0 && length <= 64 && raw <= rawMaxUnsigned(length);
}

void validateSignalValues(const CanMessage *message, const CanSignal *signal, ValidationResult &result)
{
    const QString msgName = message ? message->getName() : QString();
    const QString sigName = signal->getName();
    const int length = signal->getLength();
    const double factor = signal->getFactor();
    const double offset = signal->getOffset();
    const double minPhys = signal->getMin();
    const double maxPhys = signal->getMax();
    const double initialVal = signal->getInitialValue();

    if (factor == 0.0) {
        addError(result, msgName, sigName, QStringLiteral("Resolution（精度）不能为0"));
        return;
    }

    if (minPhys > maxPhys) {
        addError(result, msgName, sigName, QStringLiteral("物理最小值不能大于物理最大值"));
    }

    const qint64 rawMinSignedLimit = rawMinSigned(length);
    const qint64 rawMaxSignedLimit = rawMaxSigned(length);
    const quint64 rawMaxUnsignedLimit = rawMaxUnsigned(length);

    if (signal->isSigned()) {
        qint64 rawMin = static_cast<qint64>(std::llround((minPhys - offset) / factor));
        qint64 rawMax = static_cast<qint64>(std::llround((maxPhys - offset) / factor));
        if (rawMin < rawMinSignedLimit || rawMin > rawMaxSignedLimit) {
            addError(result, msgName, sigName,
                     QStringLiteral("由物理最小值换算的总线值 %1 超出有符号 %2 位范围 [%3, %4]")
                         .arg(rawMin).arg(length).arg(rawMinSignedLimit).arg(rawMaxSignedLimit));
        }
        if (rawMax < rawMinSignedLimit || rawMax > rawMaxSignedLimit) {
            addError(result, msgName, sigName,
                     QStringLiteral("由物理最大值换算的总线值 %1 超出有符号 %2 位范围 [%3, %4]")
                         .arg(rawMax).arg(length).arg(rawMinSignedLimit).arg(rawMaxSignedLimit));
        }
        if (rawMin <= rawMax) {
            qint64 initRaw = static_cast<qint64>(std::llround(initialVal));
            if (initRaw < rawMinSignedLimit || initRaw > rawMaxSignedLimit) {
                addError(result, msgName, sigName,
                         QStringLiteral("初始值(Hex) %1 超出有符号 %2 位范围 [%3, %4]")
                             .arg(initRaw).arg(length).arg(rawMinSignedLimit).arg(rawMaxSignedLimit));
            } else if (initRaw < rawMin || initRaw > rawMax) {
                addError(result, msgName, sigName,
                         QStringLiteral("初始值(Hex) %1 不在物理范围换算的总线范围 [%2, %3] 内")
                             .arg(initRaw).arg(rawMin).arg(rawMax));
            }
        }
    } else {
        double rawMinD = (minPhys - offset) / factor;
        double rawMaxD = (maxPhys - offset) / factor;
        quint64 rawMin = static_cast<quint64>(std::llround(rawMinD));
        quint64 rawMax = static_cast<quint64>(std::llround(rawMaxD));
        if (rawMinD < 0 || rawMin > rawMaxUnsignedLimit) {
            addError(result, msgName, sigName,
                     QStringLiteral("由物理最小值换算的总线值 %1 超出无符号 %2 位范围 [0, %3]")
                         .arg(static_cast<qint64>(rawMin)).arg(length).arg(rawMaxUnsignedLimit));
        }
        if (rawMaxD < 0 || rawMax > rawMaxUnsignedLimit) {
            addError(result, msgName, sigName,
                     QStringLiteral("由物理最大值换算的总线值 %1 超出无符号 %2 位范围 [0, %3]")
                         .arg(static_cast<qint64>(rawMax)).arg(length).arg(rawMaxUnsignedLimit));
        }
        quint64 initRaw = static_cast<quint64>(std::llround(initialVal));
        if (initRaw > rawMaxUnsignedLimit) {
            addError(result, msgName, sigName,
                     QStringLiteral("初始值(Hex) %1 超出无符号 %2 位范围 [0, %3]")
                         .arg(initRaw).arg(length).arg(rawMaxUnsignedLimit));
        } else if (rawMin <= rawMax && (initRaw < rawMin || initRaw > rawMax)) {
            addError(result, msgName, sigName,
                     QStringLiteral("初始值(Hex) %1 不在物理范围换算的总线范围 [%2, %3] 内")
                         .arg(initRaw).arg(rawMin).arg(rawMax));
        }
    }

    // 校验从 Excel 导入的总线最小/最大值(Hex)是否在位宽和有符号/无符号范围内
    if (signal->hasRawRange()) {
        const double rawMinHexD = signal->getRawMin();
        const double rawMaxHexD = signal->getRawMax();
        const qint64 rawMinHex = static_cast<qint64>(std::llround(rawMinHexD));
        const qint64 rawMaxHex = static_cast<qint64>(std::llround(rawMaxHexD));

        if (rawMinHex > rawMaxHex) {
            addError(result, msgName, sigName,
                     QStringLiteral("总线最小值(Hex)不能大于总线最大值(Hex)"));
        }

        if (signal->isSigned()) {
            if (!rawInSignedRange(rawMinHex, length)) {
                addError(result, msgName, sigName,
                         QStringLiteral("总线最小值(Hex) %1 超出有符号 %2 位范围 [%3, %4]")
                             .arg(rawMinHex)
                             .arg(length)
                             .arg(rawMinSignedLimit)
                             .arg(rawMaxSignedLimit));
            }
            if (!rawInSignedRange(rawMaxHex, length)) {
                addError(result, msgName, sigName,
                         QStringLiteral("总线最大值(Hex) %1 超出有符号 %2 位范围 [%3, %4]")
                             .arg(rawMaxHex)
                             .arg(length)
                             .arg(rawMinSignedLimit)
                             .arg(rawMaxSignedLimit));
            }
        } else {
            if (rawMinHex < 0 || static_cast<quint64>(rawMinHex) > rawMaxUnsignedLimit) {
                addError(result, msgName, sigName,
                         QStringLiteral("总线最小值(Hex) %1 超出无符号 %2 位范围 [0, %3]")
                             .arg(rawMinHex)
                             .arg(length)
                             .arg(rawMaxUnsignedLimit));
            }
            if (rawMaxHex < 0 || static_cast<quint64>(rawMaxHex) > rawMaxUnsignedLimit) {
                addError(result, msgName, sigName,
                         QStringLiteral("总线最大值(Hex) %1 超出无符号 %2 位范围 [0, %3]")
                             .arg(rawMaxHex)
                             .arg(length)
                             .arg(rawMaxUnsignedLimit));
            }
        }
    }

    // 校验 Invalid / Inactive Value (Hex) 是否在范围内
    const QString invalidHex = signal->getInvalidValueHex().trimmed();
    if (!invalidHex.isEmpty()) {
        bool ok = false;
        const qint64 val = parseHexToSigned(invalidHex, &ok);
        if (ok) {
            if (signal->isSigned()) {
                if (!rawInSignedRange(val, length)) {
                    addError(result, msgName, sigName,
                             QStringLiteral("无效值(Hex) %1 超出有符号 %2 位范围 [%3, %4]")
                                 .arg(val)
                                 .arg(length)
                                 .arg(rawMinSignedLimit)
                                 .arg(rawMaxSignedLimit));
                }
            } else {
                if (val < 0 || static_cast<quint64>(val) > rawMaxUnsignedLimit) {
                    addError(result, msgName, sigName,
                             QStringLiteral("无效值(Hex) %1 超出无符号 %2 位范围 [0, %3]")
                                 .arg(val)
                                 .arg(length)
                                 .arg(rawMaxUnsignedLimit));
                }
            }
        }
    }

    const QString inactiveHex = signal->getInactiveValueHex().trimmed();
    if (!inactiveHex.isEmpty()) {
        bool ok = false;
        const qint64 val = parseHexToSigned(inactiveHex, &ok);
        if (ok) {
            if (signal->isSigned()) {
                if (!rawInSignedRange(val, length)) {
                    addError(result, msgName, sigName,
                             QStringLiteral("非使能值(Hex) %1 超出有符号 %2 位范围 [%3, %4]")
                                 .arg(val)
                                 .arg(length)
                                 .arg(rawMinSignedLimit)
                                 .arg(rawMaxSignedLimit));
                }
            } else {
                if (val < 0 || static_cast<quint64>(val) > rawMaxUnsignedLimit) {
                    addError(result, msgName, sigName,
                             QStringLiteral("非使能值(Hex) %1 超出无符号 %2 位范围 [0, %3]")
                                 .arg(val)
                                 .arg(length)
                                 .arg(rawMaxUnsignedLimit));
                }
            }
        }
    }
}

using Cell = std::pair<int, int>;

// DBC 约定：@0 = Motorola（大端，startBit 为 MSB），@1 = Intel（小端，startBit 为 LSB）
// 重叠判断使用“物理位”(byte, bit_in_byte)，其中 bit_in_byte 统一为 0=LSB..7=MSB（与帧内线性编号 bit 0..7, 8..15 一致）
static int bitIndexToByte(int bitIndex) { return bitIndex / 8; }
static int bitIndexToBitInByte(int bitIndex) { return bitIndex % 8; }

// Motorola：MSB 在 startBit，先向低位延伸（7,6,...,0），再跳到下一字节高位（15,14,...,8）
static QList<Cell> signalCellsMotorola(int startBit, int length, int messageLengthBytes)
{
    QList<Cell> cells;
    int bitIndex = startBit;
    for (int k = 0; k < length; ++k) {
        int byteIdx = bitIndexToByte(bitIndex);
        int bitInByte = bitIndexToBitInByte(bitIndex);
        if (byteIdx >= 0 && byteIdx < messageLengthBytes && bitInByte >= 0 && bitInByte < 8) {
            cells.append(std::make_pair(byteIdx, bitInByte));
        }
        if (bitIndex % 8 == 0) {
            bitIndex += 15;
        } else {
            bitIndex -= 1;
        }
    }
    return cells;
}

// Intel：LSB 在 startBit，向高位延伸 startBit, startBit+1, ...
static QList<Cell> signalCellsIntel(int startBit, int length, int messageLengthBytes)
{
    QList<Cell> cells;
    for (int k = 0; k < length; ++k) {
        const int bitIndex = startBit + k;
        const int byteIdx = bitIndexToByte(bitIndex);
        const int bitInByte = bitIndexToBitInByte(bitIndex);
        if (byteIdx >= 0 && byteIdx < messageLengthBytes && bitInByte >= 0 && bitInByte < 8) {
            cells.append(std::make_pair(byteIdx, bitInByte));
        }
    }
    return cells;
}

QList<Cell> signalCells(const CanSignal *signal, int messageLengthBytes)
{
    const int startBit = signal->getStartBit();
    const int length = signal->getLength();
    const bool motorola = (signal->getByteOrder() == 0);
    return motorola ? signalCellsMotorola(startBit, length, messageLengthBytes)
                    : signalCellsIntel(startBit, length, messageLengthBytes);
}

void validateMessageOverlap(const CanMessage *message, ValidationResult &result)
{
    const QString msgName = message->getName();
    const int msgLenBytes = message->getLength();
    const QList<CanSignal *> signals = message->getSignals();

    for (int i = 0; i < signals.size(); ++i) {
        const CanSignal *sig = signals.at(i);
        const int startBit = sig->getStartBit();
        const int length = sig->getLength();
        if (length <= 0) {
            addError(result, msgName, sig->getName(), QStringLiteral("信号长度必须大于0"));
            continue;
        }
        if (startBit < 0) {
            addError(result, msgName, sig->getName(), QStringLiteral("起始位不能为负"));
            continue;
        }
        const bool motorola = (sig->getByteOrder() == 0);
        if (!motorola) {
            const int maxBit = msgLenBytes * 8 - 1;
            if (startBit + length - 1 > maxBit) {
                addError(result, msgName, sig->getName(),
                         QStringLiteral("信号位范围 [%1, %2] 超出报文长度（报文 %3 字节，有效位 0..%4）")
                             .arg(startBit).arg(startBit + length - 1).arg(msgLenBytes).arg(maxBit));
            }
        }
    }

    for (int i = 0; i < signals.size(); ++i) {
        QList<Cell> setI = signalCells(signals.at(i), msgLenBytes);
        if (setI.size() < signals.at(i)->getLength()) {
            addError(result, msgName, signals.at(i)->getName(),
                     QStringLiteral("信号位范围超出报文长度（报文 %1 字节）").arg(msgLenBytes));
        }
        std::set<Cell> uniqueI(setI.begin(), setI.end());
        if (uniqueI.size() != setI.size()) {
            addError(result, msgName, signals.at(i)->getName(), QStringLiteral("信号内部位重叠（起始位/长度与字节序不一致）"));
        }
        for (int j = i + 1; j < signals.size(); ++j) {
            QList<Cell> setJ = signalCells(signals.at(j), msgLenBytes);
            for (const Cell &c : setJ) {
                if (uniqueI.count(c)) {
                    addError(result, msgName, QString(),
                             QStringLiteral("信号 \"%1\" 与 \"%2\" 位重叠").arg(signals.at(i)->getName(), signals.at(j)->getName()));
                    break;
                }
            }
        }
    }
}

} // namespace

ValidationResult validateMessages(const QList<CanMessage *> &messages)
{
    ValidationResult result;
    for (CanMessage *msg : messages) {
        if (!msg) {
            continue;
        }
        for (CanSignal *sig : msg->getSignals()) {
            if (sig) {
                validateSignalValues(msg, sig, result);
            }
        }
        validateMessageOverlap(msg, result);
    }
    return result;
}
