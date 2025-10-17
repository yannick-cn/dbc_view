#ifndef DBCPARSER_H
#define DBCPARSER_H

#include <QString>
#include <QList>
#include <QMap>
#include "canmessage.h"

class DbcParser
{
public:
    DbcParser();
    ~DbcParser();
    
    bool parseFile(const QString &filePath);
    QList<CanMessage*> getMessages() const { return m_messages; }
    CanMessage* getMessage(int id) const;
    QString getVersion() const { return m_version; }
    QString getBusType() const { return m_busType; }
    
    void clear();

private:
    QString m_version;
    QString m_busType;
    QList<CanMessage*> m_messages;
    QMap<int, CanMessage*> m_messageMap;
    
    bool parseLine(const QString &line);
    bool parseMessage(const QString &line);
    bool parseSignal(const QString &line, CanMessage *message);
    bool parseValueTable(const QString &line);
    bool parseAttribute(const QString &line);
    
    QStringList splitDbcLine(const QString &line);
    double parseDouble(const QString &str);
    int parseInt(const QString &str);
};

#endif // DBCPARSER_H
