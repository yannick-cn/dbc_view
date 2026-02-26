#ifndef DBCWRITER_H
#define DBCWRITER_H

#include <QString>
#include <QStringList>
#include <QList>
#include <QPair>

#include "canmessage.h"
#include "dbcexcelconverter.h"

class DbcWriter
{
public:
    /** Global value tables (VAL_TABLE_): list of (name, value->description map). */
    using GlobalValueTables = QList<QPair<QString, QMap<int, QString>>>;

    static bool write(const QString &filePath,
                      const QString &version,
                      const QString &busType,
                      const QStringList &nodes,
                      const QList<CanMessage*> &messages,
                      const QString &dbComment = QString(),
                      const QString &documentTitle = QString(),
                      const QList<DbcExcelConverter::ChangeHistoryEntry> &changeHistory = QList<DbcExcelConverter::ChangeHistoryEntry>(),
                      const GlobalValueTables &globalValueTables = GlobalValueTables(),
                      QString *error = nullptr);
};

#endif // DBCWRITER_H
