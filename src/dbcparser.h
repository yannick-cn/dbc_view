#ifndef DBCPARSER_H
#define DBCPARSER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include <QtGlobal>
#include "canmessage.h"
#include "dbcexcelconverter.h"

class DbcParser
{
public:
    DbcParser();
    ~DbcParser();

    bool parseFile(const QString &filePath);
    bool loadFromExcelImport(DbcExcelConverter::ImportResult &result);
    const QList<CanMessage*> &getMessages() const { return m_messages; }
    CanMessage* getMessage(quint32 id) const;
    QString getVersion() const { return m_version; }
    QString getBusType() const { return m_busType; }
    QString getDocumentTitle() const { return m_documentTitle; }
    QList<DbcExcelConverter::ChangeHistoryEntry> getChangeHistory() const { return m_changeHistory; }
    QStringList getNodes() const { return m_nodes; }
    /** Global named value tables (VAL_TABLE_ name val "desc" ...). Order preserved. */
    QList<QPair<QString, QMap<int, QString>>> getGlobalValueTables() const { return m_globalValueTables; }

    void clear();

private:
    QString m_version;
    QString m_busType;
    QString m_documentTitle;
    QList<DbcExcelConverter::ChangeHistoryEntry> m_changeHistory;
    QStringList m_nodes;
    QList<CanMessage*> m_messages;
    QMap<quint32, CanMessage*> m_messageMap;
    /** When true, current BO_ is VECTOR__INDEPENDENT_SIG_MSG; skip adding it and its SG_ lines. */
    bool m_skipSignalsForCurrentMessage;
    QMap<QString, QStringList> m_messageAttributeEnums;
    QMap<QString, QStringList> m_signalAttributeEnums;
    QList<QPair<QString, QMap<int, QString>>> m_globalValueTables;

    bool parseLine(const QString &line);
    bool parseMessage(const QString &line);
    bool parseSignal(const QString &line);
    bool parseValueTable(const QString &line);
    bool parseGlobalValueTable(const QString &line);
    bool parseAttribute(const QString &line);
    bool parseAttributeDefinition(const QString &line);
    bool parseComment(const QString &line);
    bool parseBoTxBu(const QString &line);
    
    QStringList splitDbcLine(const QString &line);
    double parseDouble(const QString &str);
    int parseInt(const QString &str);
    QString enumValueLookup(const QMap<QString, QStringList> &map, const QString &attrName, int index) const;
};

#endif // DBCPARSER_H
