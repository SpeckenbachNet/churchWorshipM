#include "mainwin.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationVersion(QLatin1String(APP_VERSION));
    a.setWindowIcon(QIcon(":/icons/app_icon"));

    MainWin w;
    w.setWindowTitle(
        QStringLiteral(APP_NAME) +
        " " +
        QStringLiteral(APP_VERSION)
    );
    w.show();
    return a.exec();
}
