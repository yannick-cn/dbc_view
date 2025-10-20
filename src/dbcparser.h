#ifndef DBCPARSER_H
#define DBCPARSER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>
#include "canmessage.h"

class DbcParser
{
public:
    DbcParser();
    ~DbcParser();
    
    bool parseFile(const QString &filePath);
    const QList<CanMessage*> &getMessages() const { return m_messages; }
    CanMessage* getMessage(int id) const;
    QString getVersion() const { return m_version; }
    QString getBusType() const { return m_busType; }
    QStringList getNodes() const { return m_nodes; }
    
    void clear();

private:
    QString m_version;
    QString m_busType;
    QStringList m_nodes;
    QList<CanMessage*> m_messages;
    QMap<int, CanMessage*> m_messageMap;
    QMap<QString, QStringList> m_messageAttributeEnums;
    QMap<QString, QStringList> m_signalAttributeEnums;
    
    bool parseLine(const QString &line);
    bool parseMessage(const QString &line);
    bool parseSignal(const QString &line);
    bool parseValueTable(const QString &line);
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
