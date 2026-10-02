#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QApplication>
#include <QSettings>
#include <QToolButton>
#include <QElapsedTimer>
#include <QTimer>
#include <memory>

#include "mediaitem.h"
#include "slidedeck.h"
#include "beamerwindow.h"
#include "countdown.h"
#include "song.h"

class BeamerWindow;
class QListWidgetItem;
class QNetworkAccessManager;
class PresentationConverter;
class MediaLibrary;
class EventStore;
class EventHeader;
class SongStore;
class QJsonObject;
class QShortcut;
class QLabel;
class QSlider;

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

    // --- Events (the playlist belongs to the open event and is saved automatically)
    void openEventsPage();
    void openStartEvent();             // at program start: last / next upcoming event
    void loadEvent(const QString &id); // empty = no event open
    void scheduleSave();               // every change of the playlist
    void saveEvent();                  // writes a pending change right now
    void onEventsChanged();            // open event renamed or deleted
    QJsonObject entryToJson(const QListWidgetItem *item) const;
    QListWidgetItem *entryFromJson(const QJsonObject &o);
    void updateEventHeader();
    void editCurrentEvent();
    void updateButtonStates();   // enables only buttons that can do something right now
    void showPage(QWidget *page);   // switches the stacked widget (presentation, settings, bible)

    // --- Bible page
    void openBiblePage(QListWidgetItem *editItem);   // nullptr = new entry
    void applyBiblePassage();

    // --- Song library page
    void openSongsPage(bool pick);     // pick = choose songs for the playlist
    void applySongSelection();
    QString songCredits(const QListWidgetItem *item) const;   // copyright lines for the first slide
    bool refreshSongEntry(QListWidgetItem *item);   // lyrics from the library, true if changed
    QList<SongSlide> songSlides(const QListWidgetItem *item) const;   // from the library, empty if unknown
    QStringList songSlideLabels(const QListWidgetItem *item) const;   // "Vers 1", ... empty if unknown
    QStringList songTranslationSlides(const QListWidgetItem *item) const;   // empty: no translation
    void editSongEntry(QListWidgetItem *item);

    // --- Media library page
    void openLibraryPage(bool pick);   // pick = choose entries for the playlist
    void applyLibrarySelection();
    QListWidgetItem *createLibraryEntry(const QString &libraryId);
    void syncLibraryPaths();           // playlist entries follow moved / relinked library files

    void addFiles();                   // "File...": into the library and into the playlist
    void addTextEntry(MediaItem::Type type);
    void addYouTube();
    void addBlank();
    void addCountdown();
    void convertPresentations();   // queues all presentations of the playlist for conversion
    void insertEntry(QListWidgetItem *item);   // behind the current entry
    void fetchYouTubeTitle(const QString &url);
    void fetchYouTubeThumbnail(const QString &videoId);
    void editEntry(QListWidgetItem *item);
    void editEntryBackground(QListWidgetItem *item);
    void editVideoSettings(QListWidgetItem *item);
    void updateVideoTime(qint64 position);
    SlideBackground entryBackground(const QListWidgetItem *item) const;   // resolved, with current path
    SlideBackground defaultBackground(const QListWidgetItem *item) const; // song's, otherwise the event's
    void removeSelected();
    void moveSelected(int delta);
    QListWidgetItem *createEntry(const QString &title, MediaItem::Type type,
                                 const QString &source, const QString &text);

    // --- Slides
    // Slides of an entry; nullptr (with 'error') if they cannot be shown (yet)
    std::unique_ptr<SlideDeck> createDeck(QListWidgetItem *item, QString *error, bool leadingBlank) const;
    void showEntry(QListWidgetItem *item);
    void showSlide(int index);
    void nextSlide();
    void previousSlide();
    void updatePreview();
    void updateBeamer();

    // --- Loop (announcements): runs on the projector on its own while the control is used for
    // preparing. Clicks only change the preview until the loop is ended.
    // A large countdown holds the projector the same way.
    bool isLooping() const { return m_loopDeck != nullptr; }
    bool isCountdownRunning() const { return m_countdownDeck != nullptr; }
    bool isHolding() const { return isLooping() || (isCountdownRunning() && !m_countdownSettings.corner); }
    void startCountdown(QListWidgetItem *item);
    void stopCountdown();
    void countdownTick();
    void startLoop(QListWidgetItem *item, int slide);
    void stopLoop();
    void loopTick();
    void reloadLoopDeck();     // after a round: takes over changed files / texts
    QString loopSignature(const QListWidgetItem *item) const;
    void updateLiveBar();
    void goLive();             // ends loop and countdown, the projector shows the selected slide
    void editAutoAdvance(QListWidgetItem *item);

    // --- Projector
    void setBeamerVisible(bool visible);
    void setBlack(bool black);

    QToolButton *m_newBtn = nullptr;
    QToolButton *m_openBtn = nullptr;
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

    QToolButton *m_songsBtn = nullptr;          // presenter toolbar
    QToolButton *m_songsBackBtn = nullptr;      // songs toolbar
    QToolButton *m_songsNewBtn = nullptr;
    QToolButton *m_songsEditBtn = nullptr;
    QToolButton *m_songsImportBtn = nullptr;
    QToolButton *m_songsRemoveBtn = nullptr;
    QToolButton *m_songsApplyBtn = nullptr;

    QToolButton *m_eventsBackBtn = nullptr;     // events toolbar
    QToolButton *m_eventsNewBtn = nullptr;
    QToolButton *m_eventsEditBtn = nullptr;
    QToolButton *m_eventsRemoveBtn = nullptr;
    QToolButton *m_eventsImportBtn = nullptr;
    QToolButton *m_eventsExportBtn = nullptr;
    QToolButton *m_eventsOpenBtn = nullptr;

    QToolButton *m_playBtn = nullptr;
    QToolButton *m_pauseBtn = nullptr;
    QToolButton *m_stopBtn = nullptr;

    BeamerWindow *m_beamer = nullptr;

    std::unique_ptr<SlideDeck> m_deck;   // slides of the current playlist entry
    int  m_currentSlide = -1;
    bool m_startAtLastSlide = false;     // set when stepping backwards into the previous entry

    QString m_youTubeId;                 // video of the current entry (empty if no YouTube entry)
    QImage  m_videoThumb;                // preview image of a video (YouTube or local) for the control window
    QString m_localVideo;                // file of the current local video entry (empty if none)
    BeamerWindow::VideoEnd m_localVideoEnd = BeamerWindow::VideoEnd::Black;
    QWidget *m_videoRow = nullptr;       // position bar and time of local videos
    QSlider *m_videoSlider = nullptr;
    QLabel  *m_videoTime = nullptr;
    qint64   m_videoDuration = 0;
    bool     m_videoSliderUpdating = false;   // position set by the player, not by the user
    QNetworkAccessManager *m_network = nullptr;
    PresentationConverter *m_converter = nullptr;
    MediaLibrary          *m_library = nullptr;
    EventStore            *m_events = nullptr;
    SongStore             *m_songs = nullptr;
    EventHeader           *m_eventHeader = nullptr;

    QString m_eventId;                   // open event (empty = none)
    SlideBackground m_eventBackground;   // of the open event: text slides are redrawn when it changes
    SlideBackground m_shownBackground;   // of the entry shown (Inherit: no text entry)
    bool    m_loading  = false;          // suppresses saving while a playlist is loaded
    bool    m_unsaved  = false;          // playlist changed since the last save
    QTimer  m_saveTimer;                 // collects the changes of one user action into one save

    QList<QShortcut *> m_presenterShortcuts;   // only active on the presentation page

    QTimer m_previewTimer;               // debounces re-rendering while resizing

    QListWidgetItem           *m_loopItem = nullptr;   // entry of the loop (nullptr: removed meanwhile)
    std::unique_ptr<SlideDeck> m_loopDeck;             // own slides: the preview may show something else
    int                        m_loopSlide = 0;
    QString                    m_loopSignature;        // state of file / text when the deck was made
    QTimer                     m_loopTimer;
    QWidget                   *m_loopBar = nullptr;    // "On the projector: ..." above the preview
    QLabel                    *m_loopLabel = nullptr;
    QLabel                    *m_previewHint = nullptr; // "Preview only" while the projector is held
    QShortcut                 *m_goLiveShortcut = nullptr;

    QListWidgetItem              *m_countdownItem = nullptr;   // for the live preview (nullptr: removed)
    std::unique_ptr<CountdownDeck> m_countdownDeck;
    CountdownSettings             m_countdownSettings;
    QElapsedTimer                 m_countdownClock;
    int                           m_countdownShown = -1;   // seconds currently on the projector
    QTimer                        m_countdownTimer;
    bool                          m_blackAfterCountdown = false;   // until the next slide is chosen
};
#endif // MAINWIN_H
