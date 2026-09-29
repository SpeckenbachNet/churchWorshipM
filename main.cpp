#include "mainwin.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);  // required by Qt WebEngine
    QApplication a(argc, argv);
    a.setApplicationVersion(QLatin1String(APP_VERSION));
    a.setOrganizationName(QStringLiteral(COMPANY_NAME));
    a.setWindowIcon(QIcon(":/icons/app_icon"));

    // Language of the operating system; English (the source texts) if there is no translation.
    // qtbase: Qt's own texts (standard buttons, file dialog, ...)
    const QLocale locale;
    QTranslator qtTranslator;
    if (qtTranslator.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        a.installTranslator(&qtTranslator);
    }
    // Embedded by qt_add_translations as :/i18n/ChurchWorshipM_<lang>.qm
    QTranslator appTranslator;
    if (appTranslator.load(locale, QStringLiteral(APP_NAME), QStringLiteral("_"), QStringLiteral(":/i18n"))) {
        a.installTranslator(&appTranslator);
    }

    MainWin w;
    w.show();
    return a.exec();
}
