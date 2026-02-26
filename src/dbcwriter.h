#ifndef DBCWRITER_H
#define DBCWRITER_H

#include <QString>
#include <QStringList>
#include <QList>

#include "canmessage.h"
#include "dbcexcelconverter.h"

class DbcWriter
{
public:
    static bool write(const QString &filePath,
                      const QString &version,
                      const QString &busType,
                      const QStringList &nodes,
                      const QList<CanMessage*> &messages,
                      const QString &dbComment = QString(),
                      const QString &documentTitle = QString(),
                      const QList<DbcExcelConverter::ChangeHistoryEntry> &changeHistory = QList<DbcExcelConverter::ChangeHistoryEntry>(),
                      QString *error = nullptr);
};

#endif // DBCWRITER_H
