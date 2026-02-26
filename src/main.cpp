#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include "mainwindow.h"
#include "dbcparser.h"
#include "dbcvalidator.h"

int main(int argc, char *argv[])
{
    // 命令行校验模式：传入一个 .dbc 文件时，仅解析并执行重叠校验后打印结果并退出（无需 GUI）
    if (argc >= 2) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        if (path.endsWith(QStringLiteral(".dbc"), Qt::CaseInsensitive)) {
            QCoreApplication app(argc, argv);
            DbcParser parser;
            if (!parser.parseFile(path)) {
                qWarning("Failed to parse: %s", qPrintable(path));
                return 1;
            }
            const ValidationResult result = validateMessages(parser.getMessages());
            if (result.ok) {
                qWarning("Overlap validation: OK (no errors).");
                return 0;
            }
            qWarning("Overlap validation: %d error(s)", result.errors.size());
            for (const QString &e : result.errors) {
                qWarning("%s", qPrintable(e));
            }
            return result.errors.isEmpty() ? 0 : 1;
        }
    }

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}
