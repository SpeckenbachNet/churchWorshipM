#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QApplication>
#include <QSettings>
#include <QToolButton>
#include <QTimer>
#include <memory>

#include "mediaitem.h"
#include "slidedeck.h"

class BeamerWindow;
class QListWidgetItem;
class QNetworkAccessManager;
class PresentationConverter;
class MediaLibrary;
class QShortcut;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWin;
}
QT_END_NAMESPACE

class MainWin : public QMainWindow
{
    Q_OBJECT

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    MainWin(QWidget *parent = nullptr);
    ~MainWin();
    QSettings settings;

private:
    Ui::MainWin *ui;
    void initializeForm();
    void initializeConnections();
    void initializeShortcuts();

    // --- Playlist (one per event)
    void newPlaylist();
    void openPlaylist();
    bool savePlaylist();
    bool savePlaylistAs();
    bool loadPlaylistFile(const QString &path);
    bool writePlaylistFile(const QString &path);
    bool maybeSave();
    void setModified(bool modified);
    void updateWindowTitle();
    void updateButtonStates();   // enables only buttons that can do something right now
    void showPage(QWidget *page);   // switches the stacked widget (presentation, settings, bible)

    // --- Bible page
    void openBiblePage(QListWidgetItem *editItem);   // nullptr = new entry
    void applyBiblePassage();

    // --- Media library page
    void openLibraryPage(bool pick);   // pick = choose entries for the playlist
    void applyLibrarySelection();
    QListWidgetItem *createLibraryEntry(const QString &libraryId);
    void syncLibraryPaths();           // playlist entries follow moved / relinked library files

    void addFiles();                   // "File...": into the library and into the playlist
    void addTextEntry(MediaItem::Type type);
    void addYouTube();
    void addBlank();
    void convertPresentations();   // queues all presentations of the playlist for conversion
    void insertEntry(QListWidgetItem *item);   // behind the current entry
    void fetchYouTubeTitle(const QString &url);
    void fetchYouTubeThumbnail(const QString &videoId);
    void editEntry(QListWidgetItem *item);
    void removeSelected();
    void moveSelected(int delta);
    QListWidgetItem *createEntry(const QString &title, MediaItem::Type type,
                                 const QString &source, const QString &text);

    // --- Slides
    void showEntry(QListWidgetItem *item);
    void showSlide(int index);
    void nextSlide();
    void previousSlide();
    void updatePreview();
    void updateBeamer();

    // --- Projector
    void setBeamerVisible(bool visible);
    void setBlack(bool black);

    QToolButton *m_newBtn = nullptr;
    QToolButton *m_openBtn = nullptr;
    QToolButton *m_saveBtn = nullptr;
    QToolButton *m_beamerBtn = nullptr;
    QToolButton *m_blackBtn = nullptr;
    QToolButton *m_settingsBtn = nullptr;
    QToolButton *m_helpBtn = nullptr;

    QToolButton *m_addBtn = nullptr;
    QToolButton *m_editBtn = nullptr;
    QToolButton *m_removeBtn = nullptr;
    QToolButton *m_upBtn = nullptr;
    QToolButton *m_downBtn = nullptr;

    QToolButton *m_backBtn = nullptr;           // settings toolbar
    QToolButton *m_settingsHelpBtn = nullptr;

    QToolButton *m_bibleBackBtn = nullptr;      // bible toolbar
    QToolButton *m_bibleApplyBtn = nullptr;
    QListWidgetItem *m_editingBibleItem = nullptr;   // entry edited on the bible page

    QToolButton *m_libraryBtn = nullptr;        // presenter toolbar
    QToolButton *m_libraryBackBtn = nullptr;    // library toolbar
    QToolButton *m_libraryAddBtn = nullptr;
    QToolButton *m_libraryUpdateBtn = nullptr;
    QToolButton *m_libraryRemoveBtn = nullptr;
    QToolButton *m_libraryApplyBtn = nullptr;

    QToolButton *m_playBtn = nullptr;
    QToolButton *m_pauseBtn = nullptr;
    QToolButton *m_stopBtn = nullptr;

    BeamerWindow *m_beamer = nullptr;

    std::unique_ptr<SlideDeck> m_deck;   // slides of the current playlist entry
    int  m_currentSlide = -1;
    bool m_startAtLastSlide = false;     // set when stepping backwards into the previous entry

    QString m_youTubeId;                 // video of the current entry (empty if no YouTube entry)
    QImage  m_youTubeThumb;              // preview image for the control window
    QNetworkAccessManager *m_network = nullptr;
    PresentationConverter *m_converter = nullptr;
    MediaLibrary          *m_library = nullptr;

    QString m_playlistPath;
    bool    m_modified = false;
    bool    m_loading  = false;          // suppresses the modified flag while a playlist is loaded

    QList<QShortcut *> m_presenterShortcuts;   // only active on the presentation page

    QTimer m_previewTimer;               // debounces re-rendering while resizing
};
#endif // MAINWIN_H
