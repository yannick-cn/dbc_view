#include "dbcexcelconverter.h"

#include <QDateTime>
#include <QMap>
#include <QLocale>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QtGlobal>

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "miniz.h"

namespace {
QStringList headerLabels()
{
    return {
        "Msg Name\n报文名称",
        "Msg Type\n报文类型",
        "Msg ID\n报文标识符",
        "Msg Send Type\n报文发送类型",
        "Msg Cycle Time (ms)\n报文周期时间",
        "Msg Length (Byte)\n报文长度",
        "Signal Name\n信号名称",
        "Signal Description\n信号描述",
        "Byte Order\n排列格式(Intel/Motorola)",
        "Start Byte\n起始字节",
        "Start Bit\n起始位",
        "Signal Send Type\n信号发送类型",
        "Signal Length (Bit)\n信号长度",
        "Date Type\n数据类型",
        "Resolution\n精度",
        "Offset\n偏移量",
        "Signal Min. Value (Phys)\n物理最小值",
        "Signal Max. Value (Phys)\n物理最大值",
        "Signal Min. Value (Hex)\n总线最小值",
        "Signal Max. Value (Hex)\n总线最大值",
        "Initial Value (Hex)\n初始值",
        "Invalid Value (Hex)\n无效值",
        "Inactive Value (Hex)\n非使能值",
        "Unit\n单位",
        "Signal Value Description\n信号值描述",
        "Msg Cycle Time Fast(ms)\n报文发送的快速周期(ms)",
        "Msg Nr. Of Repetition\n报文快速发送的次数",
        "Msg Delay Time(ms)\n报文延时时间",
        "ADC"
    };
}

QString columnName(int index)
{
    QString result;
    int number = index;
    while (number > 0) {
        int remainder = (number - 1) % 26;
        result.prepend(QChar('A' + remainder));
        number = (number - 1) / 26;
    }
    return result;
}

QString cellReference(int row, int column)
{
    return columnName(column) + QString::number(row);
}

QString doubleToString(double value)
{
    return QLocale::c().toString(value, 'g', 15);
}

quint64 maskForLength(int length)
{
    if (length <= 0) {
        return 0;
    }
    if (length >= 64) {
        return std::numeric_limits<quint64>::max();
    }
    return (quint64(1) << length) - 1;
}

quint64 physicalToRawMasked(const CanSignal *signal, double physicalValue)
{
    if (signal->getFactor() == 0.0) {
        return 0;
    }
    const double raw = (physicalValue - signal->getOffset()) / signal->getFactor();
    const qint64 rawInt = static_cast<qint64>(std::llround(raw));
    const quint64 mask = maskForLength(signal->getLength());
    return static_cast<quint64>(rawInt) & mask;
}

QString formatHex(quint64 value)
{
    return QString("0x%1").arg(value, 0, 16).toUpper();
}

QString formatValueTable(const QMap<int, QString> &valueTable)
{
    if (valueTable.isEmpty()) {
        return QString();
    }
    QStringList lines;
    for (auto it = valueTable.cbegin(); it != valueTable.cend(); ++it) {
        lines.append(QString("%1: %2").arg(formatHex(static_cast<quint64>(static_cast<qint64>(it.key()))), it.value()));
    }
    return lines.join('\n');
}

QByteArray generateStylesXml()
{
    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.setAutoFormatting(false);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement("styleSheet");
    writer.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");

    writer.writeStartElement("fonts");
    writer.writeAttribute("count", "3");

    auto writeFont = [&writer](bool bold, const QString &colorRgb) {
        auto writeValElement = [&writer](const char *tag, const QString &value) {
            writer.writeStartElement(tag);
            writer.writeAttribute("val", value);
            writer.writeEndElement();
        };

        writer.writeStartElement("font");
        if (bold) {
            writer.writeEmptyElement("b");
        }
        writeValElement("sz", QStringLiteral("11"));
        if (!colorRgb.isEmpty()) {
            writer.writeStartElement("color");
            writer.writeAttribute("rgb", colorRgb);
            writer.writeEndElement();
        }
        writeValElement("name", QStringLiteral("Calibri"));
        writeValElement("family", QStringLiteral("2"));
        writer.writeEndElement();
    };

    writeFont(false, QString());           // Default font
    writeFont(true, QStringLiteral("FFFFFFFF")); // Header font
    writeFont(true, QString());            // Message font (bold)
    writer.writeEndElement(); // fonts

    writer.writeStartElement("fills");
    writer.writeAttribute("count", "5");
    // Fill 0 - required default
    writer.writeStartElement("fill");
    writer.writeEmptyElement("patternFill");
    writer.writeEndElement();
    // Fill 1 - required gray125
    writer.writeStartElement("fill");
    writer.writeStartElement("patternFill");
    writer.writeAttribute("patternType", "gray125");
    writer.writeEndElement();
    writer.writeEndElement();

    auto writeSolidFill = [&writer](const QString &rgb) {
        writer.writeStartElement("fill");
        writer.writeStartElement("patternFill");
        writer.writeAttribute("patternType", "solid");
        writer.writeStartElement("fgColor");
        writer.writeAttribute("rgb", rgb);
        writer.writeEndElement();
        writer.writeStartElement("bgColor");
        writer.writeAttribute("indexed", "64");
        writer.writeEndElement();
        writer.writeEndElement();
        writer.writeEndElement();
    };

    writeSolidFill(QStringLiteral("FF0096D6")); // Header fill
    writeSolidFill(QStringLiteral("FFCCE7F5")); // Message fill
    writeSolidFill(QStringLiteral("FFFFFFFF")); // Signal fill
    writer.writeEndElement(); // fills

    writer.writeStartElement("borders");
    writer.writeAttribute("count", "1");
    writer.writeStartElement("border");
    writer.writeEmptyElement("left");
    writer.writeEmptyElement("right");
    writer.writeEmptyElement("top");
    writer.writeEmptyElement("bottom");
    writer.writeEmptyElement("diagonal");
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("cellStyleXfs");
    writer.writeAttribute("count", "1");
    writer.writeStartElement("xf");
    writer.writeAttribute("numFmtId", "0");
    writer.writeAttribute("fontId", "0");
    writer.writeAttribute("fillId", "0");
    writer.writeAttribute("borderId", "0");
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("cellXfs");
    writer.writeAttribute("count", "4");
    // 0 default
    writer.writeStartElement("xf");
    writer.writeAttribute("numFmtId", "0");
    writer.writeAttribute("fontId", "0");
    writer.writeAttribute("fillId", "0");
    writer.writeAttribute("borderId", "0");
    writer.writeAttribute("xfId", "0");
    writer.writeEndElement();
    // 1 header
    writer.writeStartElement("xf");
    writer.writeAttribute("numFmtId", "0");
    writer.writeAttribute("fontId", "1");
    writer.writeAttribute("fillId", "2");
    writer.writeAttribute("borderId", "0");
    writer.writeAttribute("xfId", "0");
    writer.writeAttribute("applyFill", "1");
    writer.writeAttribute("applyFont", "1");
    writer.writeAttribute("applyAlignment", "1");
    writer.writeStartElement("alignment");
    writer.writeAttribute("horizontal", "center");
    writer.writeAttribute("vertical", "center");
    writer.writeAttribute("wrapText", "1");
    writer.writeEndElement();
    writer.writeEndElement();
    // 2 message
    writer.writeStartElement("xf");
    writer.writeAttribute("numFmtId", "0");
    writer.writeAttribute("fontId", "2");
    writer.writeAttribute("fillId", "3");
    writer.writeAttribute("borderId", "0");
    writer.writeAttribute("xfId", "0");
    writer.writeAttribute("applyFill", "1");
    writer.writeAttribute("applyFont", "1");
    writer.writeAttribute("applyAlignment", "1");
    writer.writeStartElement("alignment");
    writer.writeAttribute("vertical", "center");
    writer.writeAttribute("wrapText", "1");
    writer.writeEndElement();
    writer.writeEndElement();
    // 3 signal
    writer.writeStartElement("xf");
    writer.writeAttribute("numFmtId", "0");
    writer.writeAttribute("fontId", "0");
    writer.writeAttribute("fillId", "4");
    writer.writeAttribute("borderId", "0");
    writer.writeAttribute("xfId", "0");
    writer.writeAttribute("applyFill", "1");
    writer.writeAttribute("applyAlignment", "1");
    writer.writeStartElement("alignment");
    writer.writeAttribute("vertical", "center");
    writer.writeAttribute("wrapText", "1");
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("cellStyles");
    writer.writeAttribute("count", "1");
    writer.writeStartElement("cellStyle");
    writer.writeAttribute("name", "Normal");
    writer.writeAttribute("xfId", "0");
    writer.writeAttribute("builtinId", "0");
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndDocument();
    return data;
}

QByteArray generateContentTypesXml()
{
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "<Override PartName=\"/docProps/core.xml\" ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>"
        "<Override PartName=\"/docProps/app.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>"
        "</Types>";
    return QByteArray(xml);
}

QByteArray generateRootRels()
{
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
        "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
        "</Relationships>";
    return QByteArray(xml);
}

QByteArray generateWorkbookRels()
{
    static const char xml[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";
    return QByteArray(xml);
}

QByteArray generateWorkbookXml()
{
    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement("workbook");
    writer.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    writer.writeNamespace("http://schemas.openxmlformats.org/officeDocument/2006/relationships", "r");

    writer.writeStartElement("bookViews");
    writer.writeStartElement("workbookView");
    writer.writeAttribute("tabRatio", "600");
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("sheets");
    writer.writeStartElement("sheet");
    writer.writeAttribute("name", "Sheet1");
    writer.writeAttribute("sheetId", "1");
    writer.writeAttribute("r:id", "rId1");
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndDocument();
    return data;
}

QByteArray generateCoreProps()
{
    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement("cp:coreProperties");
    writer.writeNamespace("http://schemas.openxmlformats.org/package/2006/metadata/core-properties", "cp");
    writer.writeNamespace("http://purl.org/dc/elements/1.1/", "dc");
    writer.writeNamespace("http://purl.org/dc/terms/", "dcterms");
    writer.writeNamespace("http://www.w3.org/2001/XMLSchema-instance", "xsi");
    writer.writeTextElement("dc:creator", "DBCViewer");
    writer.writeTextElement("cp:lastModifiedBy", "DBCViewer");
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + "Z";
    writer.writeStartElement("dcterms:created");
    writer.writeAttribute("xsi:type", "dcterms:W3CDTF");
    writer.writeCharacters(timestamp);
    writer.writeEndElement();
    writer.writeStartElement("dcterms:modified");
    writer.writeAttribute("xsi:type", "dcterms:W3CDTF");
    writer.writeCharacters(timestamp);
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return data;
}

QByteArray generateAppProps()
{
    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement("Properties");
    writer.writeDefaultNamespace("http://schemas.openxmlformats.org/officeDocument/2006/extended-properties");
    writer.writeNamespace("http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes", "vt");
    writer.writeTextElement("Application", "Qt");
    writer.writeStartElement("HeadingPairs");
    writer.writeStartElement("vt:vector");
    writer.writeAttribute("size", "2");
    writer.writeAttribute("baseType", "variant");
    writer.writeStartElement("vt:variant");
    writer.writeTextElement("vt:lpstr", "Worksheets");
    writer.writeEndElement();
    writer.writeStartElement("vt:variant");
    writer.writeTextElement("vt:i4", "1");
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeStartElement("TitlesOfParts");
    writer.writeStartElement("vt:vector");
    writer.writeAttribute("size", "1");
    writer.writeAttribute("baseType", "lpstr");
    writer.writeTextElement("vt:lpstr", "Sheet1");
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndDocument();
    return data;
}

void writeInlineStringCell(QXmlStreamWriter &writer, int row, int column, int style, const QString &value)
{
    if (value.isEmpty()) {
        return;
    }
    writer.writeStartElement("c");
    writer.writeAttribute("r", cellReference(row, column));
    writer.writeAttribute("t", "inlineStr");
    if (style >= 0) {
        writer.writeAttribute("s", QString::number(style));
    }
    writer.writeStartElement("is");
    writer.writeStartElement("t");
    writer.writeCharacters(value);
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();
}

void writeNumericCell(QXmlStreamWriter &writer, int row, int column, int style, double value)
{
    writer.writeStartElement("c");
    writer.writeAttribute("r", cellReference(row, column));
    if (style >= 0) {
        writer.writeAttribute("s", QString::number(style));
    }
    writer.writeStartElement("v");
    writer.writeCharacters(doubleToString(value));
    writer.writeEndElement();
    writer.writeEndElement();
}

QByteArray generateWorksheetXml(const QList<CanMessage*> &messages)
{
    const QStringList headers = headerLabels();
    const int columnCount = headers.size();

    QByteArray data;
    QXmlStreamWriter writer(&data);
    writer.writeStartDocument(QStringLiteral("1.0"), true);
    writer.writeStartElement("worksheet");
    writer.writeDefaultNamespace("http://schemas.openxmlformats.org/spreadsheetml/2006/main");
    writer.writeNamespace("http://schemas.openxmlformats.org/officeDocument/2006/relationships", "r");

    writer.writeStartElement("sheetPr");
    writer.writeStartElement("outlinePr");
    writer.writeAttribute("summaryBelow", "0");
    writer.writeAttribute("summaryRight", "1");
    writer.writeEndElement();
    writer.writeEndElement();

    int totalRows = 1; // header
    for (const CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        totalRows += 1; // message row
        totalRows += message->getSignals().size();
    }
    if (totalRows < 1) {
        totalRows = 1;
    }

    writer.writeStartElement("dimension");
    writer.writeAttribute("ref", QString("A1:%1%2").arg(columnName(columnCount)).arg(totalRows));
    writer.writeEndElement();

    writer.writeStartElement("sheetViews");
    writer.writeStartElement("sheetView");
    writer.writeAttribute("tabSelected", "1");
    writer.writeAttribute("workbookViewId", "0");
    writer.writeStartElement("pane");
    writer.writeAttribute("xSplit", "6");
    writer.writeAttribute("ySplit", "1");
    writer.writeAttribute("topLeftCell", "G2");
    writer.writeAttribute("activePane", "bottomRight");
    writer.writeAttribute("state", "frozen");
    writer.writeEndElement();
    writer.writeEndElement();
    writer.writeEndElement();

    writer.writeStartElement("sheetFormatPr");
    writer.writeAttribute("defaultRowHeight", "15");
    writer.writeEndElement();

    writer.writeStartElement("cols");
    for (int col = 1; col <= columnCount; ++col) {
        writer.writeStartElement("col");
        writer.writeAttribute("min", QString::number(col));
        writer.writeAttribute("max", QString::number(col));
        writer.writeAttribute("width", col <= 6 ? "22" : "24");
        writer.writeAttribute("customWidth", "1");
        writer.writeEndElement();
    }
    writer.writeEndElement();

    writer.writeStartElement("sheetData");

    int currentRow = 1;

    writer.writeStartElement("row");
    writer.writeAttribute("r", QString::number(currentRow));
    writer.writeAttribute("s", "1");
    writer.writeAttribute("customFormat", "1");
    writer.writeAttribute("ht", "30");
    writer.writeAttribute("customHeight", "1");
    for (int col = 1; col <= columnCount; ++col) {
        writeInlineStringCell(writer, currentRow, col, 1, headers.at(col - 1));
    }
    writer.writeEndElement();

    for (const CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        ++currentRow;
        writer.writeStartElement("row");
        writer.writeAttribute("r", QString::number(currentRow));
        writer.writeAttribute("s", "2");
        writer.writeAttribute("customFormat", "1");
        writer.writeAttribute("ht", "24");
        writer.writeAttribute("customHeight", "1");
        writeInlineStringCell(writer, currentRow, 1, 2, message->getName());
        writeInlineStringCell(writer, currentRow, 2, 2,
            message->getMessageType().isEmpty() ? message->getFrameFormat() : message->getMessageType());
        writeInlineStringCell(writer, currentRow, 3, 2, QString("0x%1").arg(message->getId(), 0, 16).toUpper());
        writeInlineStringCell(writer, currentRow, 4, 2, message->getSendType());
        if (message->getCycleTime() != 0) {
            writeNumericCell(writer, currentRow, 5, 2, message->getCycleTime());
        }
        writeNumericCell(writer, currentRow, 6, 2, message->getLength());
        if (!message->getComment().isEmpty()) {
            writeInlineStringCell(writer, currentRow, 8, 2, message->getComment());
        }
        if (message->getCycleTimeFast() != 0) {
            writeNumericCell(writer, currentRow, 26, 2, message->getCycleTimeFast());
        }
        if (message->getNrOfRepetitions() != 0) {
            writeNumericCell(writer, currentRow, 27, 2, message->getNrOfRepetitions());
        }
        if (message->getDelayTime() != 0) {
            writeNumericCell(writer, currentRow, 28, 2, message->getDelayTime());
        }
        writeInlineStringCell(writer, currentRow, 29, 2, message->getTransmitter());
        writer.writeEndElement();

        const QList<CanSignal*> messageSignals = message->getSignals();

        for (const CanSignal *signal : messageSignals) {
            if (!signal) {
                continue;
            }
            ++currentRow;
            writer.writeStartElement("row");
            writer.writeAttribute("r", QString::number(currentRow));
            writer.writeAttribute("outlineLevel", "1");
            writer.writeAttribute("hidden", "1");
            writer.writeAttribute("s", "3");
            writer.writeAttribute("customFormat", "1");

            writeInlineStringCell(writer, currentRow, 7, 3, signal->getName());
            writeInlineStringCell(writer, currentRow, 8, 3, signal->getDescription());
            writeInlineStringCell(writer, currentRow, 9, 3, signal->getByteOrder() == 0 ? "Intel LSB" : "Motorola MSB");
            writeNumericCell(writer, currentRow, 10, 3, signal->getStartBit() / 8);
            writeNumericCell(writer, currentRow, 11, 3, signal->getStartBit() % 8);
            writeInlineStringCell(writer, currentRow, 12, 3, signal->getSendType());
            writeNumericCell(writer, currentRow, 13, 3, signal->getLength());
            writeInlineStringCell(writer, currentRow, 14, 3, signal->isSigned() ? "signed" : "unsigned");
            writeNumericCell(writer, currentRow, 15, 3, signal->getFactor());
            writeNumericCell(writer, currentRow, 16, 3, signal->getOffset());
            writeNumericCell(writer, currentRow, 17, 3, signal->getMin());
            writeNumericCell(writer, currentRow, 18, 3, signal->getMax());
            writeInlineStringCell(writer, currentRow, 19, 3, formatHex(physicalToRawMasked(signal, signal->getMin())));
            writeInlineStringCell(writer, currentRow, 20, 3, formatHex(physicalToRawMasked(signal, signal->getMax())));
            writeInlineStringCell(writer, currentRow, 21, 3,
                formatHex(static_cast<quint64>(std::llround(signal->getInitialValue())) & maskForLength(signal->getLength())));
            writeInlineStringCell(writer, currentRow, 22, 3, signal->getInvalidValueHex());
            writeInlineStringCell(writer, currentRow, 23, 3, signal->getInactiveValueHex());
            writeInlineStringCell(writer, currentRow, 24, 3, signal->getUnit());
            writeInlineStringCell(writer, currentRow, 25, 3, formatValueTable(signal->getValueTable()));
            writeInlineStringCell(writer, currentRow, 29, 3, signal->getReceiversAsString());
            writer.writeEndElement();
        }

        // Outline metadata is encoded via row attributes above; Excel will expose group controls automatically.
    }

    writer.writeEndElement();

    writer.writeStartElement("pageMargins");
    writer.writeAttribute("left", "0.7");
    writer.writeAttribute("right", "0.7");
    writer.writeAttribute("top", "0.75");
    writer.writeAttribute("bottom", "0.75");
    writer.writeAttribute("header", "0.3");
    writer.writeAttribute("footer", "0.3");
    writer.writeEndElement();

    writer.writeEndElement();
    writer.writeEndDocument();
    return data;
}

bool writeZipArchive(const QString &filePath, const QList<QPair<QString, QByteArray>> &entries, QString *error)
{
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_writer_init_file(&archive, filePath.toUtf8().constData(), 0)) {
        if (error) {
            *error = QString("Failed to initialize archive writer for %1").arg(filePath);
        }
        return false;
    }

    for (const auto &entry : entries) {
        if (!mz_zip_writer_add_mem(&archive, entry.first.toUtf8().constData(), entry.second.constData(), entry.second.size(), MZ_BEST_COMPRESSION)) {
            if (error) {
                *error = QString("Failed to add %1 to archive").arg(entry.first);
            }
            mz_zip_writer_end(&archive);
            return false;
        }
    }

    const bool ok = mz_zip_writer_finalize_archive(&archive);
    mz_zip_writer_end(&archive);
    if (!ok) {
        if (error) {
            *error = QString("Failed to finalize Excel archive %1").arg(filePath);
        }
        return false;
    }
    return true;
}

QByteArray readZipEntry(const QString &filePath, const QString &entryName, QString *error)
{
    mz_zip_archive archive;
    memset(&archive, 0, sizeof(archive));
    if (!mz_zip_reader_init_file(&archive, filePath.toUtf8().constData(), 0)) {
        if (error) {
            *error = QString("Failed to open %1 as zip archive").arg(filePath);
        }
        return QByteArray();
    }

    size_t size = 0;
    void *buffer = mz_zip_reader_extract_file_to_heap(&archive, entryName.toUtf8().constData(), &size, 0);
    mz_zip_reader_end(&archive);
    if (!buffer || size == 0) {
        if (error) {
            *error = QString("Missing entry %1 in %2").arg(entryName, filePath);
        }
        if (buffer) {
            mz_free(buffer);
        }
        return QByteArray();
    }
    QByteArray data(static_cast<const char*>(buffer), static_cast<int>(size));
    mz_free(buffer);
    return data;
}

QString normalizeSendType(const QString &value, bool isSignal)
{
    static const QStringList msgTypes = {
        "Cycle","OnChange","OnWrite","OnWriteWithRepetition","OnChangeWithRepetition","IfActive","IfActiveWithRepetition","NoMsgSendType"
    };
    static const QStringList sigTypes = {
        "Cyclic","OnWrite","OnWriteWithRepetition","OnChange","OnChangeWithRepetition","IfActive","IfActiveWithRepetition","NoSigSendType"
    };
    const QStringList &choices = isSignal ? sigTypes : msgTypes;
    const int idx = choices.indexOf(value, Qt::CaseInsensitive);
    if (idx >= 0) {
        return choices.at(idx);
    }
    return value;
}

QStringList splitLines(const QString &text)
{
    QString normalized = text;
    normalized.replace('\r', '\n');
    QStringList parts = normalized.split('\n', QString::SkipEmptyParts);
    for (QString &part : parts) {
        part = part.trimmed();
    }
    return parts;
}

int parseHexToInt(const QString &text, bool *ok)
{
    QString trimmed = text.trimmed();
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        const int value = trimmed.mid(2).toInt(ok, 16);
        return value;
    }
    return trimmed.toInt(ok, 10);
}

quint64 parseHexToUInt64(const QString &text, bool *ok)
{
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        if (ok) {
            *ok = true;
        }
        return 0;
    }
    if (trimmed.startsWith("0x", Qt::CaseInsensitive)) {
        return trimmed.mid(2).toULongLong(ok, 16);
    }
    return trimmed.toULongLong(ok, 10);
}

} // namespace

void DbcExcelConverter::ImportResult::clear()
{
    for (CanMessage *message : messages) {
        if (!message) {
            continue;
        }
        for (CanSignal *signal : message->getSignals()) {
            delete signal;
        }
        delete message;
    }
    messages.clear();
}

bool DbcExcelConverter::exportToExcel(const QString &filePath,
                                      const QString &version,
                                      const QString &busType,
                                      const QStringList &nodes,
                                      const QList<CanMessage*> &messages,
                                      QString *error)
{
    Q_UNUSED(version);
    Q_UNUSED(busType);
    Q_UNUSED(nodes);

    QList<QPair<QString, QByteArray>> entries;
    entries.append({QStringLiteral("[Content_Types].xml"), generateContentTypesXml()});
    entries.append({QStringLiteral("_rels/.rels"), generateRootRels()});
    entries.append({QStringLiteral("xl/_rels/workbook.xml.rels"), generateWorkbookRels()});
    entries.append({QStringLiteral("xl/workbook.xml"), generateWorkbookXml()});
    entries.append({QStringLiteral("xl/styles.xml"), generateStylesXml()});
    entries.append({QStringLiteral("docProps/core.xml"), generateCoreProps()});
    entries.append({QStringLiteral("docProps/app.xml"), generateAppProps()});
    entries.append({QStringLiteral("xl/worksheets/sheet1.xml"), generateWorksheetXml(messages)});

    return writeZipArchive(filePath, entries, error);
}

bool DbcExcelConverter::importFromExcel(const QString &filePath,
                                        ImportResult &result,
                                        QString *error)
{
    result.clear();
    const QByteArray sheetXml = readZipEntry(filePath, QStringLiteral("xl/worksheets/sheet1.xml"), error);
    if (sheetXml.isEmpty()) {
        return false;
    }

    const QStringList expectedHeaders = headerLabels();
    const int columnCount = expectedHeaders.size();

    QXmlStreamReader reader(sheetXml);
    QMap<int, QMap<int, QString>> table; // row -> column -> value
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QLatin1String("row")) {
            const int rowIndex = reader.attributes().value("r").toInt();
            while (!(reader.isEndElement() && reader.name() == QLatin1String("row"))) {
                reader.readNext();
                if (reader.isStartElement() && reader.name() == QLatin1String("c")) {
                    const QString cellRef = reader.attributes().value("r").toString();
                    QString letters;
                    for (const QChar ch : cellRef) {
                        if (ch.isLetter()) {
                            letters.append(ch);
                        } else {
                            break;
                        }
                    }
                    int column = 0;
                    for (QChar ch : letters) {
                        column = column * 26 + (ch.unicode() - 'A' + 1);
                    }

                    QString value;
                    if (reader.attributes().value("t") == QLatin1String("inlineStr")) {
                        while (!(reader.isEndElement() && reader.name() == QLatin1String("c"))) {
                            reader.readNext();
                            if (reader.isStartElement() && reader.name() == QLatin1String("t")) {
                                value = reader.readElementText();
                            }
                        }
                    } else {
                        while (!(reader.isEndElement() && reader.name() == QLatin1String("c"))) {
                            reader.readNext();
                            if (reader.isStartElement() && reader.name() == QLatin1String("v")) {
                                value = reader.readElementText();
                            }
                        }
                    }

                    table[rowIndex][column] = value;
                }
            }
        }
    }

    if (reader.hasError()) {
        if (error) {
            *error = QString("Failed to parse sheet XML: %1").arg(reader.errorString());
        }
        return false;
    }

    if (table.isEmpty()) {
        if (error) {
            *error = "Excel sheet is empty";
        }
        return false;
    }

    // Validate headers in row 1
    const QMap<int, QString> headerRow = table.value(1);
    for (int col = 1; col <= columnCount; ++col) {
        const QString value = headerRow.value(col).trimmed();
        if (value != expectedHeaders.at(col - 1)) {
            if (error) {
                *error = QString("Unexpected header in column %1: %2").arg(col).arg(value);
            }
            return false;
        }
    }

    CanMessage *currentMessage = nullptr;
    QStringList nodeAccumulator;

    for (auto it = table.begin(); it != table.end(); ++it) {
        if (it.key() == 1) {
            continue;
        }
        const QMap<int, QString> row = it.value();
        const QString messageName = row.value(1).trimmed();
        const QString signalName = row.value(7).trimmed();

        if (!messageName.isEmpty()) {
            currentMessage = new CanMessage();
            currentMessage->setName(messageName);
            currentMessage->setMessageType(row.value(2).trimmed());
            const QString idText = row.value(3).trimmed();
            bool idOk = false;
            currentMessage->setId(parseHexToUInt64(idText, &idOk));
            currentMessage->setSendType(normalizeSendType(row.value(4).trimmed(), false));
            currentMessage->setCycleTime(row.value(5).toInt());
            currentMessage->setLength(row.value(6).toInt());
            currentMessage->setComment(row.value(8));
            currentMessage->setCycleTimeFast(row.value(26).toInt());
            currentMessage->setNrOfRepetitions(row.value(27).toInt());
            currentMessage->setDelayTime(row.value(28).toInt());
            currentMessage->setTransmitter(row.value(29).trimmed());
            currentMessage->setFrameFormat(currentMessage->getMessageType());

            if (!currentMessage->getTransmitter().isEmpty()) {
                nodeAccumulator.append(currentMessage->getTransmitter());
            }

            result.messages.append(currentMessage);
            continue;
        }

        if (!signalName.isEmpty() && currentMessage) {
            auto *signal = new CanSignal();
            signal->setName(signalName);
            signal->setDescription(row.value(8));
            const QString byteOrder = row.value(9).toLower();
            signal->setByteOrder(byteOrder.contains("motorola") ? 1 : 0);
            const int startByte = row.value(10).toInt();
            const int startBit = row.value(11).toInt();
            signal->setStartBit(startByte * 8 + startBit);
            signal->setSendType(normalizeSendType(row.value(12).trimmed(), true));
            signal->setLength(row.value(13).toInt());
            const QString dataType = row.value(14).toLower();
            signal->setSigned(dataType.contains("signed") && !dataType.contains("unsigned"));
            signal->setFactor(row.value(15).toDouble());
            signal->setOffset(row.value(16).toDouble());
            signal->setMin(row.value(17).toDouble());
            signal->setMax(row.value(18).toDouble());
            signal->setUnit(row.value(24).trimmed());

            bool initOk = false;
            const quint64 init = parseHexToUInt64(row.value(21), &initOk);
            signal->setInitialValue(initOk ? static_cast<double>(init) : 0.0);
            signal->setInvalidValueHex(row.value(22).trimmed());
            signal->setInactiveValueHex(row.value(23).trimmed());

            const QString receivers = row.value(29).trimmed();
            const QStringList receiverList = receivers.split(QRegularExpression(QStringLiteral("[,\\s]+")), QString::SkipEmptyParts);
            signal->setReceivers(receiverList);
            for (const QString &receiver : receiverList) {
                nodeAccumulator.append(receiver);
            }

            const QStringList valueLines = splitLines(row.value(25));
            if (!valueLines.isEmpty()) {
                QMap<int, QString> valueTable;
                for (const QString &line : valueLines) {
                    const int colonIndex = line.indexOf(':');
                    if (colonIndex <= 0) {
                        continue;
                    }
                    bool valueOk = false;
                    const int rawValue = parseHexToInt(line.left(colonIndex), &valueOk);
                    if (!valueOk) {
                        continue;
                    }
                    valueTable[rawValue] = line.mid(colonIndex + 1).trimmed();
                }
                signal->setValueTable(valueTable);
            }

            currentMessage->addSignal(signal);
        }
    }

    nodeAccumulator.removeDuplicates();
    result.nodes = nodeAccumulator;
    result.busType = "CAN";
    for (const CanMessage *message : std::as_const(result.messages)) {
        if (message && message->getMessageType().contains("CANFD", Qt::CaseInsensitive)) {
            result.busType = "CAN FD";
            break;
        }
    }
    result.version = "Generated by Excel Import";
    return true;
}
