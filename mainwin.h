#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QApplication>
#include <QMetaEnum>
#include <QLocale>
#include <QtMath>
#include <QPageSize>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>
#include <QToolButton>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QMovie>
#include <qlabel.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWin;
}
QT_END_NAMESPACE

class MainWin : public QMainWindow
{
    Q_OBJECT

protected:
    void resizeEvent(QResizeEvent *event);

public:
    MainWin(QWidget *parent = nullptr);
    ~MainWin();
    QSettings settings;
    void readSettingsFromINI();

private:
    Ui::MainWin *ui;
    void initSettings();
    void initializeForm();
    void initializeConnections();
    void initializeSettings();
    void updateListIconSize();
    void refreshSettings();

    QString getPageFormat();
    QStringList m_processedFiles;
    QLabel *m_loadingLabel;
    QToolButton *m_scanBtn = nullptr;
    QToolButton *m_settingsBtn = nullptr;
    QToolButton *m_refreshBtn = nullptr;
    QToolButton *m_rotateBtn = nullptr;
    QToolButton *m_clearBtn = nullptr;
    QToolButton *m_fakeDplxBtn = nullptr;
    QToolButton *m_saveSelBtn = nullptr;
    QToolButton *m_saveAllBtn = nullptr;

    QString m_cachedScannerOutput;

    QGraphicsScene* m_previewScene;      // Die "Leinwand"
    QGraphicsPixmapItem* m_previewItem;  // Das eigentliche Bild-Objekt auf der Leinwand
};
#endif // MAINWIN_H
