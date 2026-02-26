#ifndef DBCVALIDATOR_H
#define DBCVALIDATOR_H

#include <QList>
#include <QStringList>

class CanMessage;

struct ValidationResult
{
    bool ok = true;
    QStringList errors;
};

ValidationResult validateMessages(const QList<CanMessage *> &messages);

#endif // DBCVALIDATOR_H
