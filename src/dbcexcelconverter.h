#ifndef DBCEXCELCONVERTER_H
#define DBCEXCELCONVERTER_H

#include <QString>
#include <QStringList>
#include <QList>

#include "canmessage.h"

class DbcExcelConverter
{
public:
    struct ImportResult
    {
        QString version;
        QString busType;
        QString documentTitle;
        QStringList nodes;
        QList<CanMessage*> messages;

        void clear();
    };

    static bool exportToExcel(const QString &filePath,
                              const QString &version,
                              const QString &busType,
                              const QStringList &nodes,
                              const QList<CanMessage*> &messages,
                              const QString &documentTitle = QString(),
                              QString *error = nullptr);

    static bool importFromExcel(const QString &filePath,
                                ImportResult &result,
                                QString *error = nullptr);
};

#endif // DBCEXCELCONVERTER_H
