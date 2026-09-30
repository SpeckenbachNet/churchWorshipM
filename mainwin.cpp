#include "mainwin.h"
#include "./ui_mainwin.h"
#include "toolbarm.h"
#include "beamerwindow.h"
#include "playlistdelegate.h"
#include "textslidedialog.h"
#include "presentationconverter.h"
#include "biblepage.h"
#include "librarypage.h"
#include "medialibrary.h"
#include "eventspage.h"
#include "eventstore.h"
#include "eventheader.h"
#include "eventdialog.h"
#include "songspage.h"
#include "songstore.h"
#include "songeditor.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QShortcut>

namespace {
constexpr QSize kThumbSize{160, 90};
}

MainWin::MainWin(QWidget *parent)
    : QMainWindow(parent),
    ui(new Ui::MainWin)
{
    ui->setupUi(this);
    m_beamer = new BeamerWindow;
    m_network = new QNetworkAccessManager(this);
    m_converter = new PresentationConverter(this);
    m_library = new MediaLibrary(m_converter, this);
    ui->libraryBrowser->setLibrary(m_library);
    m_events = new EventStore(this);
    ui->eventsBrowser->setStore(m_events);
    m_songs = new SongStore(this);
    ui->songsBrowser->setStore(m_songs);

    initializeForm();
    initializeConnections();
    initializeShortcuts();
    openStartEvent();
}

// ====== Initializing / Settings  ======

void MainWin::initializeForm() {
    // --- Main toolbar
    m_newBtn  = ui->presenterToolbar->addButton("newBtn",  tr("New"),    ":icons/calendar_event_new",  true);
    m_newBtn->setToolTip(tr("New event (Ctrl+N)"));
    m_openBtn = ui->presenterToolbar->addButton("openBtn", tr("Events"), ":icons/calendar", true);
    m_openBtn->setToolTip(tr("Open or manage events and templates (Ctrl+O)"));

    ui->presenterToolbar->addSpacer();

    m_beamerBtn = ui->presenterToolbar->addButton("beamerBtn", tr("Projector"), ":icons/monitor", true);
    m_beamerBtn->setCheckable(true);
    m_beamerBtn->setToolTip(tr("Show / hide projector output (F5)"));

    m_blackBtn = ui->presenterToolbar->addButton("blackBtn", tr("Black"), ":icons/black_screen", true);
    m_blackBtn->setCheckable(true);
    m_blackBtn->setToolTip(tr("Blank the projector (B)"));

    ui->presenterToolbar->addSpacer();  // Spacer for align buttons right

    m_songsBtn = ui->presenterToolbar->addButton("songsBtn", "", ":icons/type_song");
    m_songsBtn->setToolTip(tr("Song library"));
    m_libraryBtn = ui->presenterToolbar->addButton("libraryBtn", "", ":icons/library");
    m_libraryBtn->setToolTip(tr("Media library"));
    m_settingsBtn = ui->presenterToolbar->addButton("settingsBtn", "", ":icons/settings");
    m_settingsBtn->setToolTip(tr("Settings"));
    m_helpBtn = ui->presenterToolbar->addButton("helpBtn", "", ":icons/help");
    m_helpBtn->setEnabled(false);

    // --- Header of the open event above the playlist
    m_eventHeader = new EventHeader(ui->leftPanel);
    ui->leftPanelLayout->insertWidget(0, m_eventHeader);

    // --- Settings toolbar
    m_backBtn = ui->settingsToolbar->addButton("backBtn", tr("Back"), ":icons/back", true);
    m_backBtn->setToolTip(tr("Back to the presentation"));
    ui->settingsToolbar->addSpacer();
    m_settingsHelpBtn = ui->settingsToolbar->addButton("settingsHelpBtn", "", ":icons/help");
    m_settingsHelpBtn->setEnabled(false);

    // --- Bible toolbar
    m_bibleBackBtn = ui->bibleToolbar->addButton("bibleBackBtn", tr("Back"), ":icons/back", true);
    m_bibleBackBtn->setToolTip(tr("Back to the presentation without changes"));
    ui->bibleToolbar->addSpacer();
    m_bibleApplyBtn = ui->bibleToolbar->addButton("bibleApplyBtn", tr("Apply"), ":icons/check", true);
    m_bibleApplyBtn->setToolTip(tr("Put the selected passage into the playlist"));

    // --- Library toolbar
    m_libraryBackBtn   = ui->libraryToolbar->addButton("libraryBackBtn", tr("Back"), ":icons/back", true);
    m_libraryAddBtn    = ui->libraryToolbar->addButton("libraryAddBtn", tr("Add"), ":icons/add", true);
    m_libraryUpdateBtn = ui->libraryToolbar->addButton("libraryUpdateBtn", tr("Update"), ":icons/refresh", true);
    m_libraryUpdateBtn->setToolTip(tr("Take over the newer version of the original file"));
    m_libraryRemoveBtn = ui->libraryToolbar->addButton("libraryRemoveBtn", tr("Remove"), ":icons/remove", true);
    ui->libraryToolbar->addSpacer();
    m_libraryApplyBtn  = ui->libraryToolbar->addButton("libraryApplyBtn", tr("Apply"), ":icons/check", true);
    m_libraryApplyBtn->setToolTip(tr("Put the selected entries into the playlist"));

    // --- Songs toolbar
    m_songsBackBtn   = ui->songsToolbar->addButton("songsBackBtn", tr("Back"), ":icons/back", true);
    m_songsBackBtn->setToolTip(tr("Back to the presentation"));
    m_songsNewBtn    = ui->songsToolbar->addButton("songsNewBtn", tr("New"), ":icons/add", true);
    m_songsNewBtn->setToolTip(tr("Type in a new song"));
    m_songsEditBtn   = ui->songsToolbar->addButton("songsEditBtn", tr("Edit"), ":icons/edit", true);
    m_songsEditBtn->setToolTip(tr("Lyrics, parts and order of the song"));
    m_songsImportBtn = ui->songsToolbar->addButton("songsImportBtn", tr("Import"), ":icons/download", true);
    m_songsImportBtn->setToolTip(tr("Import lyrics files downloaded from SongSelect (*.txt)"));
    m_songsRemoveBtn = ui->songsToolbar->addButton("songsRemoveBtn", tr("Delete"), ":icons/remove", true);
    ui->songsToolbar->addSpacer();
    m_songsApplyBtn  = ui->songsToolbar->addButton("songsApplyBtn", tr("Apply"), ":icons/check", true);
    m_songsApplyBtn->setToolTip(tr("Put the selected songs into the playlist"));

    // --- Events toolbar
    m_eventsBackBtn   = ui->eventsToolbar->addButton("eventsBackBtn", tr("Back"), ":icons/back", true);
    m_eventsBackBtn->setToolTip(tr("Back to the presentation"));
    m_eventsNewBtn    = ui->eventsToolbar->addButton("eventsNewBtn", tr("New"), ":icons/add", true);
    m_eventsEditBtn   = ui->eventsToolbar->addButton("eventsEditBtn", tr("Properties"), ":icons/edit", true);
    m_eventsEditBtn->setToolTip(tr("Name, date, time and note"));
    m_eventsRemoveBtn = ui->eventsToolbar->addButton("eventsRemoveBtn", tr("Delete"), ":icons/remove", true);
    m_eventsImportBtn = ui->eventsToolbar->addButton("eventsImportBtn", tr("Import"), ":icons/download", true);
    m_eventsImportBtn->setToolTip(tr("Import events from files (*.cwm)"));
    m_eventsExportBtn = ui->eventsToolbar->addButton("eventsExportBtn", tr("Export"), ":icons/upload", true);
    m_eventsExportBtn->setToolTip(tr("Save the selected event as a file, e.g. for another computer"));
    ui->eventsToolbar->addSpacer();
    m_eventsOpenBtn   = ui->eventsToolbar->addButton("eventsOpenBtn", tr("Open"), ":icons/check", true);
    m_eventsOpenBtn->setToolTip(tr("Open the selected event in the presentation"));

    auto *newMenu = new QMenu(m_eventsNewBtn);
    newMenu->addAction(tr("Event..."), ui->eventsBrowser, &EventsPage::newEvent);
    newMenu->addAction(tr("Template..."), ui->eventsBrowser, &EventsPage::newTemplate);
    m_eventsNewBtn->setMenu(newMenu);
    m_eventsNewBtn->setPopupMode(QToolButton::InstantPopup);

    // --- Playlist toolbar
    m_addBtn    = ui->playlistToolbar->addButton("addBtn",    "", ":icons/add");
    m_editBtn   = ui->playlistToolbar->addButton("editBtn",   "", ":icons/edit");
    m_removeBtn = ui->playlistToolbar->addButton("removeBtn", "", ":icons/remove");
    ui->playlistToolbar->addSpacer();
    m_upBtn     = ui->playlistToolbar->addButton("upBtn",     "", ":icons/arrow_up");
    m_downBtn   = ui->playlistToolbar->addButton("downBtn",   "", ":icons/arrow_down");

    m_addBtn->setToolTip(tr("Add"));
    m_editBtn->setToolTip(tr("Edit"));
    m_removeBtn->setToolTip(tr("Remove"));
    m_upBtn->setToolTip(tr("Move up"));
    m_downBtn->setToolTip(tr("Move down"));

    auto *addMenu = new QMenu(m_addBtn);
    addMenu->addAction(QIcon::fromTheme("document-open"), tr("File..."), this, &MainWin::addFiles);
    addMenu->addAction(QIcon(":icons/library"), tr("From media library..."), this, [this] {
        openLibraryPage(true);
    });
    addMenu->addAction(MediaItem::typeIcon(MediaItem::YouTube), tr("YouTube video..."),
                       this, &MainWin::addYouTube);
    addMenu->addSeparator();
    addMenu->addAction(MediaItem::typeIcon(MediaItem::Blank), tr("Blank entry"),
                       this, &MainWin::addBlank);
    addMenu->addSeparator();
    addMenu->addAction(MediaItem::typeIcon(MediaItem::Song), tr("Song..."),
                       this, [this] { openSongsPage(true); });
    addMenu->addAction(MediaItem::typeIcon(MediaItem::Bible), tr("Bible text..."),
                       this, [this] { openBiblePage(nullptr); });
    addMenu->addAction(MediaItem::typeIcon(MediaItem::Custom), tr("Own slide..."),
                       this, [this] { addTextEntry(MediaItem::Custom); });
    m_addBtn->setMenu(addMenu);
    m_addBtn->setPopupMode(QToolButton::InstantPopup);

    // --- Video controls (only visible for YouTube entries)
    m_playBtn  = ui->videoToolbar->addButton("playBtn",  tr("Play"),  ":icons/play",  true);
    m_pauseBtn = ui->videoToolbar->addButton("pauseBtn", tr("Pause"), ":icons/pause", true);
    m_stopBtn  = ui->videoToolbar->addButton("stopBtn",  tr("Stop"),  ":icons/stop",  true);
    ui->videoToolbar->addSpacer();
    ui->videoToolbar->hide();

    // --- Playlist
    ui->playlistWidget->setItemDelegate(new PlaylistDelegate(ui->playlistWidget));
    ui->playlistWidget->setDragDropMode(QAbstractItemView::InternalMove);
    ui->playlistWidget->setDefaultDropAction(Qt::MoveAction);
    ui->playlistWidget->setEditTriggers(QAbstractItemView::EditKeyPressed);  // F2 renames

    // --- Slide thumbnails
    QListWidget *slides = ui->slidesListWidget;
    slides->setViewMode(QListView::IconMode);
    slides->setFlow(QListView::LeftToRight);
    slides->setWrapping(false);
    slides->setMovement(QListView::Static);
    slides->setIconSize(kThumbSize);
    slides->setSpacing(4);
    slides->setFixedHeight(kThumbSize.height() + 2 * fontMetrics().height() + 24);
    slides->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    slides->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->previewLabel->installEventFilter(this);

    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes({380, 820});

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(60);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(200);
}

void MainWin::initializeConnections() {
    connect(m_newBtn,  &QToolButton::clicked, ui->eventsBrowser, &EventsPage::newEvent);
    connect(m_openBtn, &QToolButton::clicked, this, &MainWin::openEventsPage);
    connect(m_eventsBackBtn,   &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_eventsEditBtn,   &QToolButton::clicked, ui->eventsBrowser, &EventsPage::editSelected);
    connect(m_eventsRemoveBtn, &QToolButton::clicked, ui->eventsBrowser, &EventsPage::removeSelected);
    connect(m_eventsImportBtn, &QToolButton::clicked, ui->eventsBrowser, &EventsPage::importFiles);
    connect(m_eventsExportBtn, &QToolButton::clicked, ui->eventsBrowser, &EventsPage::exportSelected);
    connect(m_eventsOpenBtn,   &QToolButton::clicked, this, [this] {
        const QString id = ui->eventsBrowser->selectedId();
        if (!id.isEmpty()) {
            loadEvent(id);
            showPage(ui->presenterPage);
        }
    });
    connect(ui->eventsBrowser, &EventsPage::selectionChanged, this, &MainWin::updateButtonStates);
    connect(ui->eventsBrowser, &EventsPage::openRequested, this, [this](const QString &id) {
        loadEvent(id);
        showPage(ui->presenterPage);
    });
    connect(m_events, &EventStore::changed, this, &MainWin::onEventsChanged);
    connect(m_eventHeader, &EventHeader::clicked, this, &MainWin::editCurrentEvent);
    // Context and data only from header and model: the list clears itself while the window
    // is destroyed, when `ui` is already gone
    QAbstractItemModel *playlistModel = ui->playlistWidget->model();
    EventHeader *header = m_eventHeader;
    const auto updateCount = [header, playlistModel] { header->setEntryCount(playlistModel->rowCount()); };
    connect(playlistModel, &QAbstractItemModel::rowsInserted, header, updateCount);
    connect(playlistModel, &QAbstractItemModel::rowsRemoved,  header, updateCount);
    connect(playlistModel, &QAbstractItemModel::modelReset,   header, updateCount);
    connect(&m_saveTimer, &QTimer::timeout, this, &MainWin::saveEvent);
    connect(m_settingsBtn, &QToolButton::clicked, this, [this] { showPage(ui->settingsPage); });
    connect(m_backBtn,     &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_bibleBackBtn,  &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_songsBtn,       &QToolButton::clicked, this, [this] { openSongsPage(false); });
    connect(m_songsBackBtn,   &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_songsNewBtn,    &QToolButton::clicked, ui->songsBrowser, &SongsPage::newSong);
    connect(m_songsEditBtn,   &QToolButton::clicked, ui->songsBrowser, &SongsPage::editSelected);
    connect(m_songsImportBtn, &QToolButton::clicked, ui->songsBrowser, &SongsPage::importFiles);
    // Corrected lyrics appear in the open event right away
    connect(m_songs, &SongStore::changed, this, [this] {
        for (int i = 0; i < ui->playlistWidget->count(); ++i) {
            QListWidgetItem *item = ui->playlistWidget->item(i);
            if (refreshSongEntry(item) && item == ui->playlistWidget->currentItem()) {
                showEntry(item);
            }
        }
    });
    connect(m_songsRemoveBtn, &QToolButton::clicked, ui->songsBrowser, &SongsPage::removeSelected);
    connect(m_songsApplyBtn,  &QToolButton::clicked, this, &MainWin::applySongSelection);
    connect(ui->songsBrowser, &SongsPage::selectionChanged, this, &MainWin::updateButtonStates);
    connect(ui->songsBrowser, &SongsPage::pickRequested, this, &MainWin::applySongSelection);
    connect(m_libraryBtn,       &QToolButton::clicked, this, [this] { openLibraryPage(false); });
    connect(m_libraryBackBtn,   &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_libraryAddBtn,    &QToolButton::clicked, ui->libraryBrowser, &LibraryPage::addFiles);
    connect(m_libraryUpdateBtn, &QToolButton::clicked, ui->libraryBrowser, &LibraryPage::updateSelected);
    connect(m_libraryRemoveBtn, &QToolButton::clicked, ui->libraryBrowser, &LibraryPage::removeSelected);
    connect(m_libraryApplyBtn,  &QToolButton::clicked, this, &MainWin::applyLibrarySelection);
    connect(ui->libraryBrowser, &LibraryPage::selectionChanged, this, &MainWin::updateButtonStates);
    connect(ui->libraryBrowser, &LibraryPage::pickRequested, this, &MainWin::applyLibrarySelection);
    connect(m_library, &MediaLibrary::changed, this, &MainWin::syncLibraryPaths);
    connect(m_bibleApplyBtn, &QToolButton::clicked, this, &MainWin::applyBiblePassage);
    connect(ui->bibleBrowser, &BiblePage::selectionChanged, this, &MainWin::updateButtonStates);
    connect(ui->bibleBrowser, &BiblePage::settingsRequested, this, [this] { showPage(ui->settingsPage); });
    connect(m_beamerBtn, &QToolButton::toggled, this, &MainWin::setBeamerVisible);
    connect(m_blackBtn,  &QToolButton::toggled, this, &MainWin::setBlack);

    connect(m_editBtn,   &QToolButton::clicked, this, [this] { editEntry(ui->playlistWidget->currentItem()); });
    connect(m_removeBtn, &QToolButton::clicked, this, &MainWin::removeSelected);
    connect(m_upBtn,     &QToolButton::clicked, this, [this] { moveSelected(-1); });
    connect(m_downBtn,   &QToolButton::clicked, this, [this] { moveSelected(+1); });

    connect(m_playBtn,  &QToolButton::clicked, m_beamer, &BeamerWindow::playVideo);
    connect(m_pauseBtn, &QToolButton::clicked, m_beamer, &BeamerWindow::pauseVideo);
    connect(m_stopBtn,  &QToolButton::clicked, m_beamer, &BeamerWindow::stopVideo);

    connect(ui->playlistWidget, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current) { showEntry(current); });
    connect(ui->playlistWidget, &QListWidget::itemDoubleClicked, this, &MainWin::editEntry);
    connect(ui->slidesListWidget, &QListWidget::currentRowChanged, this, &MainWin::showSlide);

    // Every change of the playlist model is saved automatically (incl. drag & drop)
    QAbstractItemModel *model = ui->playlistWidget->model();
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWin::scheduleSave);
    connect(model, &QAbstractItemModel::rowsRemoved,  this, &MainWin::scheduleSave);
    connect(model, &QAbstractItemModel::rowsMoved,    this, &MainWin::scheduleSave);
    connect(model, &QAbstractItemModel::dataChanged,  this, &MainWin::scheduleSave);

    // Presentation converted in the background -> show it if it is still the current entry
    connect(m_converter, &PresentationConverter::finished, this,
            [this](const QString &source, const QString &error) {
        QListWidgetItem *item = ui->playlistWidget->currentItem();
        if (!item || MediaItem::Type(item->data(MediaItem::TypeRole).toInt()) != MediaItem::PowerPoint
            || item->data(MediaItem::SourceRole).toString() != source) {
            return;
        }
        if (error.isEmpty()) {
            showEntry(item);
        } else {
            ui->previewLabel->setText(error);
        }
    });

    // Button states follow selection and playlist content
    connect(ui->playlistWidget, &QListWidget::currentRowChanged, this, &MainWin::updateButtonStates);
    connect(model, &QAbstractItemModel::rowsInserted, this, &MainWin::updateButtonStates);
    connect(model, &QAbstractItemModel::rowsRemoved,  this, &MainWin::updateButtonStates);
    connect(model, &QAbstractItemModel::rowsMoved,    this, &MainWin::updateButtonStates);

    connect(&m_previewTimer, &QTimer::timeout, this, &MainWin::updatePreview);
}

void MainWin::initializeShortcuts() {
    // Shortcuts of the presentation page; disabled on the settings page,
    // otherwise they would steal the arrow keys from the settings widgets
    auto presenterShortcut = [this](const QKeySequence &key, QWidget *parent, auto slot) {
        auto *shortcut = new QShortcut(key, parent);
        connect(shortcut, &QShortcut::activated, this, slot);
        m_presenterShortcuts << shortcut;
    };

    // Presenter remotes send PageUp/PageDown or the arrow keys
    presenterShortcut(Qt::Key_Right,    this, &MainWin::nextSlide);
    presenterShortcut(Qt::Key_PageDown, this, &MainWin::nextSlide);
    presenterShortcut(Qt::Key_Left,     this, &MainWin::previousSlide);
    presenterShortcut(Qt::Key_PageUp,   this, &MainWin::previousSlide);
    presenterShortcut(QKeySequence::Open, this, &MainWin::openEventsPage);
    presenterShortcut(QKeySequence::New,  this, [this] { ui->eventsBrowser->newEvent(); });
    presenterShortcut(QKeySequence::Delete, ui->playlistWidget, &MainWin::removeSelected);

    // Projector control works everywhere, also while the settings are open
    connect(new QShortcut(Qt::Key_B, this), &QShortcut::activated, m_blackBtn, &QToolButton::toggle);
    connect(new QShortcut(Qt::Key_F5, this), &QShortcut::activated, m_beamerBtn, &QToolButton::toggle);
}

// ====== Playlist ======

QListWidgetItem *MainWin::createEntry(const QString &title, MediaItem::Type type,
                                      const QString &source, const QString &text) {
    auto *item = new QListWidgetItem(title);
    item->setData(MediaItem::TypeRole, int(type));
    item->setData(MediaItem::SourceRole, source);
    item->setData(MediaItem::TextRole, text);
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
}

void MainWin::insertEntry(QListWidgetItem *item) {
    const int row = ui->playlistWidget->currentRow();
    ui->playlistWidget->insertItem(row < 0 ? ui->playlistWidget->count() : row + 1, item);
    ui->playlistWidget->setCurrentItem(item);
}

void MainWin::addFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add files"), settings.value("lastMediaDir").toString(), MediaItem::mediaFileFilter());
    if (files.isEmpty()) {
        return;
    }
    settings.setValue("lastMediaDir", QFileInfo(files.first()).absolutePath());
    const bool link = settings.value("library/linkByDefault", false).toBool();

    // Every file goes into the media library first; the playlist refers to the library entry
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList errors;
    for (const QString &file : files) {
        QString error;
        const QString id = m_library->addFile(file, link, &error);
        if (id.isEmpty()) {
            errors << error;
            continue;
        }
        insertEntry(createLibraryEntry(id));   // behind the current entry, keeps the order
    }
    QApplication::restoreOverrideCursor();

    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Add files"), errors.join('\n'));
    }
}

void MainWin::addTextEntry(MediaItem::Type type) {
    TextSlideDialog dlg(type, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    // Insert behind the current entry, spontaneous slides are needed "right now"
    insertEntry(createEntry(dlg.title(), dlg.type(), {}, dlg.text()));
}

void MainWin::addYouTube() {
    bool ok = false;
    const QString url = QInputDialog::getText(this, tr("YouTube video"),
                                              tr("Link of the YouTube video:"),
                                              QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || url.isEmpty()) {
        return;
    }
    if (MediaItem::youTubeId(url).isEmpty()) {
        QMessageBox::warning(this, tr("YouTube video"), tr("This is not a valid YouTube link."));
        return;
    }
    insertEntry(createEntry(tr("YouTube video"), MediaItem::YouTube, url, {}));
    fetchYouTubeTitle(url);
}

void MainWin::convertPresentations() {
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        const QListWidgetItem *item = ui->playlistWidget->item(i);
        if (MediaItem::Type(item->data(MediaItem::TypeRole).toInt()) == MediaItem::PowerPoint) {
            m_converter->convert(item->data(MediaItem::SourceRole).toString());
        }
    }
}

void MainWin::addBlank() {
    insertEntry(createEntry(tr("Blank"), MediaItem::Blank, {}, {}));
}

void MainWin::fetchYouTubeTitle(const QString &url) {
    // oEmbed delivers the title without an API key
    QUrl request(QStringLiteral("https://www.youtube.com/oembed"));
    request.setQuery(QUrlQuery{{"url", url}, {"format", "json"}});

    QNetworkReply *reply = m_network->get(QNetworkRequest(request));
    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        reply->deleteLater();
        const QString title = QJsonDocument::fromJson(reply->readAll()).object().value("title").toString();
        if (title.isEmpty()) {
            return;
        }
        // The entry may have been moved or removed in the meantime -> search it again
        for (int i = 0; i < ui->playlistWidget->count(); ++i) {
            QListWidgetItem *item = ui->playlistWidget->item(i);
            if (item->data(MediaItem::SourceRole).toString() == url
                && item->text() == tr("YouTube video")) {
                item->setText(title);
            }
        }
    });
}

void MainWin::fetchYouTubeThumbnail(const QString &videoId) {
    const QUrl url(QStringLiteral("https://img.youtube.com/vi/%1/hqdefault.jpg").arg(videoId));
    QNetworkReply *reply = m_network->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, videoId] {
        reply->deleteLater();
        if (videoId != m_youTubeId) {
            return;  // user has already moved on
        }
        m_youTubeThumb = QImage::fromData(reply->readAll());
        updatePreview();
    });
}

void MainWin::editEntry(QListWidgetItem *item) {
    if (!item) {
        return;
    }
    const auto type = MediaItem::Type(item->data(MediaItem::TypeRole).toInt());
    if (!MediaItem::isTextType(type)) {
        ui->playlistWidget->editItem(item);  // files: only rename
        return;
    }
    if (type == MediaItem::Bible && !item->data(MediaItem::BibleRole).toJsonObject().isEmpty()) {
        openBiblePage(item);
        return;
    }
    if (type == MediaItem::Song
        && m_songs->song(item->data(MediaItem::SongRole).toJsonObject().value("id").toString()).isValid()) {
        editSongEntry(item);
        return;
    }

    TextSlideDialog dlg(type, this);
    dlg.setTitle(item->text());
    dlg.setText(item->data(MediaItem::TextRole).toString());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    item->setText(dlg.title());
    item->setData(MediaItem::TextRole, dlg.text());

    if (item == ui->playlistWidget->currentItem()) {
        showEntry(item);
    }
}

void MainWin::removeSelected() {
    const int row = ui->playlistWidget->currentRow();
    if (row < 0) {
        return;
    }
    delete ui->playlistWidget->takeItem(row);
}

void MainWin::moveSelected(int delta) {
    QListWidget *list = ui->playlistWidget;
    const int row = list->currentRow();
    const int target = row + delta;
    if (row < 0 || target < 0 || target >= list->count()) {
        return;
    }
    // Same entry stays selected -> no reload of the slides
    const QSignalBlocker blocker(list);
    list->insertItem(target, list->takeItem(row));
    list->setCurrentRow(target);
    updateButtonStates();
}

// ====== Event management ======

void MainWin::openEventsPage() {
    saveEvent();   // the list shows the current number of entries
    ui->eventsBrowser->setCurrentId(m_eventId);
    showPage(ui->eventsPage);
}

void MainWin::openStartEvent() {
    // The last opened event, unless it is over and a newer one has been planned since
    EventInfo e = m_events->event(settings.value("lastEvent").toString());
    if (!e.isValid() || e.isPast()) {
        const EventInfo next = m_events->nextUpcoming();
        if (next.isValid()) {
            e = next;
        }
    }
    loadEvent(e.id);   // none yet: the presentation page shows a hint to create one
}

void MainWin::loadEvent(const QString &id) {
    saveEvent();   // pending changes belong to the previous event

    m_loading = true;
    ui->playlistWidget->clear();
    m_eventId = m_events->event(id).isValid() ? id : QString();
    for (const QJsonValue &v : m_events->items(m_eventId)) {
        ui->playlistWidget->addItem(entryFromJson(v.toObject()));
    }
    m_loading = false;
    convertPresentations();

    if (!m_eventId.isEmpty()) {
        settings.setValue("lastEvent", m_eventId);
    }
    ui->eventsBrowser->setCurrentId(m_eventId);
    updateEventHeader();
    updateButtonStates();

    if (ui->playlistWidget->count() > 0) {
        ui->playlistWidget->setCurrentRow(0);
    } else {
        showEntry(nullptr);
    }
}

void MainWin::scheduleSave() {
    if (!m_loading && !m_eventId.isEmpty()) {
        m_unsaved = true;
        m_saveTimer.start();
    }
}

void MainWin::saveEvent() {
    // Called by the timer and directly (switching events, closing): the flag, not the
    // timer state, tells whether there is something to save
    if (!m_unsaved) {
        return;
    }
    m_unsaved = false;
    m_saveTimer.stop();

    QJsonArray items;
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        items.append(entryToJson(ui->playlistWidget->item(i)));
    }
    if (!m_events->setItems(m_eventId, items)) {
        QMessageBox::warning(this, tr("Save event"), tr("The changes of the event could not be saved."));
    }
}

void MainWin::onEventsChanged() {
    if (!m_eventId.isEmpty() && !m_events->event(m_eventId).isValid()) {
        m_unsaved = false;    // deleted: nothing to save anymore
        m_saveTimer.stop();
        loadEvent({});
        return;
    }
    updateEventHeader();   // may have been renamed
}

QJsonObject MainWin::entryToJson(const QListWidgetItem *item) const {
    const auto type = MediaItem::Type(item->data(MediaItem::TypeRole).toInt());
    QJsonObject o{{"title", item->text()}, {"type", MediaItem::typeKey(type)}};
    if (MediaItem::isTextType(type)) {
        o.insert("text", item->data(MediaItem::TextRole).toString());
        const QJsonObject bible = item->data(MediaItem::BibleRole).toJsonObject();
        if (!bible.isEmpty()) {
            o.insert("bible", bible);
        }
        const QJsonObject song = item->data(MediaItem::SongRole).toJsonObject();
        if (!song.isEmpty()) {
            o.insert("song", song);
        }
    } else {
        o.insert("source", item->data(MediaItem::SourceRole).toString());
        const QString libraryId = item->data(MediaItem::LibraryRole).toString();
        if (!libraryId.isEmpty()) {
            o.insert("library", libraryId);
        }
    }
    return o;
}

QListWidgetItem *MainWin::entryFromJson(const QJsonObject &o) {
    QListWidgetItem *item = createEntry(o.value("title").toString(),
                                        MediaItem::typeFromKey(o.value("type").toString()),
                                        o.value("source").toString(),
                                        o.value("text").toString());
    if (o.contains("bible")) {
        item->setData(MediaItem::BibleRole, o.value("bible").toObject());
    }
    if (o.contains("song")) {
        item->setData(MediaItem::SongRole, o.value("song").toObject());
        refreshSongEntry(item);   // corrections made in the library since the last time
    }
    // Library entries: the file location comes from the library ("source" is only a fallback)
    const QString libraryId = o.value("library").toString();
    if (!libraryId.isEmpty()) {
        item->setData(MediaItem::LibraryRole, libraryId);
        const LibraryEntry e = m_library->entry(libraryId);
        if (e.isValid()) {
            item->setData(MediaItem::SourceRole, e.path());
        }
    }
    return item;
}

void MainWin::updateButtonStates() {
    const int row   = ui->playlistWidget->currentRow();
    const int count = ui->playlistWidget->count();
    const bool hasEntry = row >= 0;

    m_blackBtn->setEnabled(m_beamerBtn->isChecked());

    m_addBtn->setEnabled(!m_eventId.isEmpty());   // entries always belong to an event
    m_editBtn->setEnabled(hasEntry);
    m_removeBtn->setEnabled(hasEntry);
    m_upBtn->setEnabled(row > 0);
    m_downBtn->setEnabled(hasEntry && row < count - 1);

    m_bibleApplyBtn->setEnabled(ui->bibleBrowser->passage().isValid());

    const bool librarySelection = !ui->libraryBrowser->selectedIds().isEmpty();
    m_libraryUpdateBtn->setEnabled(ui->libraryBrowser->canUpdateSelection());
    m_libraryRemoveBtn->setEnabled(librarySelection);
    m_libraryApplyBtn->setEnabled(librarySelection);

    const bool songSelection = !ui->songsBrowser->selectedIds().isEmpty();
    m_songsRemoveBtn->setEnabled(songSelection);
    m_songsEditBtn->setEnabled(ui->songsBrowser->selectedIds().size() == 1);
    m_songsApplyBtn->setEnabled(songSelection && !m_eventId.isEmpty());

    const bool eventSelection = !ui->eventsBrowser->selectedId().isEmpty();
    m_eventsEditBtn->setEnabled(eventSelection);
    m_eventsRemoveBtn->setEnabled(eventSelection);
    m_eventsExportBtn->setEnabled(eventSelection);
    m_eventsOpenBtn->setEnabled(eventSelection);
}

void MainWin::showPage(QWidget *page) {
    ui->mainStack->setCurrentWidget(page);
    const bool presenter = page == ui->presenterPage;
    for (QShortcut *shortcut : std::as_const(m_presenterShortcuts)) {
        shortcut->setEnabled(presenter);
    }
    if (page != ui->biblePage) {
        m_editingBibleItem = nullptr;
    }
    updateButtonStates();
}

// ====== Song library page ======

void MainWin::openSongsPage(bool pick) {
    ui->songsBrowser->setPickMode(pick);
    m_songsApplyBtn->setVisible(pick);
    showPage(ui->songsPage);
}

void MainWin::applySongSelection() {
    const QStringList ids = ui->songsBrowser->selectedIds();
    if (ids.isEmpty() || m_eventId.isEmpty()) {
        return;
    }
    showPage(ui->presenterPage);
    for (const QString &id : ids) {
        const Song song = m_songs->song(id);
        if (!song.isValid()) {
            continue;
        }
        // The text is a copy: the event stays complete even if the song is changed or deleted.
        // The reference allows own orders per event later on.
        QListWidgetItem *item = createEntry(song.title, MediaItem::Song, {}, QString());
        // Credits are kept with the entry as well: needed on the slides even without the library
        item->setData(MediaItem::SongRole, QJsonObject{{"id", song.id}, {"authors", song.authors},
                                                       {"copyright", song.copyright},
                                                       {"ccli", song.ccliNumber}});
        refreshSongEntry(item);   // lyrics, parts in other languages
        insertEntry(item);
    }
}

QList<SongSlide> MainWin::songSlides(const QListWidgetItem *item) const {
    const QJsonObject o = item->data(MediaItem::SongRole).toJsonObject();
    const Song song = m_songs->song(o.value("id").toString());
    if (!song.isValid()) {
        return {};
    }
    QStringList order;
    for (const QJsonValue &v : o.value("order").toArray()) {
        order << v.toString();
    }
    const QList<Song> linked = m_songs->translations(song.id);
    Song translation;
    const QString translationId = o.value("translation").toObject().value("id").toString();
    for (const Song &s : linked) {
        if (s.id == translationId) {
            translation = s;
        }
    }
    return song.slides(order, linked, translation.isValid() ? &translation : nullptr);
}

bool MainWin::refreshSongEntry(QListWidgetItem *item) {
    QJsonObject o = item->data(MediaItem::SongRole).toJsonObject();
    const Song song = m_songs->song(o.value("id").toString());
    if (!song.isValid()) {
        return false;   // not (any more) in the library: the entry keeps its copy
    }
    const QList<SongSlide> slides = songSlides(item);
    bool changed = false;
    const QString text = Song::slideText(slides);
    if (item->data(MediaItem::TextRole).toString() != text) {
        item->setData(MediaItem::TextRole, text);
        changed = true;
    }

    const auto creditsOf = [](const Song &s) {
        return QJsonObject{{"id", s.id}, {"title", s.title}, {"authors", s.authors},
                           {"copyright", s.copyright}, {"ccli", s.ccliNumber}};
    };
    QJsonObject fresh = o;
    // Translation shown below: a copy as well, one text per slide
    const Song translation = m_songs->song(o.value("translation").toObject().value("id").toString());
    if (translation.isValid()) {
        QStringList below;
        for (const SongSlide &slide : slides) {
            below << slide.below;
        }
        QJsonObject t = creditsOf(translation);
        t.insert("slides", QJsonArray::fromStringList(below));
        fresh.insert("translation", t);
    }
    // Other songs with parts in the order (verse sung in another language): their credits
    QJsonArray others;
    QStringList seen{song.id, translation.id};
    for (const SongSlide &slide : slides) {
        if (!seen.contains(slide.songId)) {
            seen << slide.songId;
            others.append(creditsOf(m_songs->song(slide.songId)));
        }
    }
    if (others.isEmpty()) {
        fresh.remove("others");
    } else {
        fresh.insert("others", others);
    }
    if (fresh != o) {
        item->setData(MediaItem::SongRole, fresh);
        changed = true;
    }
    return changed;
}

QStringList MainWin::songTranslationSlides(const QListWidgetItem *item) const {
    QStringList slides;
    const QJsonObject t = item->data(MediaItem::SongRole).toJsonObject().value("translation").toObject();
    for (const QJsonValue &v : t.value("slides").toArray()) {
        slides << v.toString();
    }
    return slides;
}

QStringList MainWin::songSlideLabels(const QListWidgetItem *item) const {
    QStringList labels;
    for (const SongSlide &slide : songSlides(item)) {
        labels << (slide.foreign ? QStringLiteral("%1 (%2)").arg(slide.part.label(), Song::languageName(slide.language))
                                 : slide.part.label());
    }
    return labels;
}

void MainWin::editSongEntry(QListWidgetItem *item) {
    QJsonObject o = item->data(MediaItem::SongRole).toJsonObject();
    const Song old = m_songs->song(o.value("id").toString());
    QStringList order;
    for (const QJsonValue &v : o.value("order").toArray()) {
        order << v.toString();
    }

    SongEditorDialog dlg(old, order, SongEditorDialog::Event, m_songs, this);
    dlg.setTranslationId(o.value("translation").toObject().value("id").toString());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    // Lyrics and data: always into the library. Order: only here, unless it becomes the default.
    Song edited = dlg.song();
    const QStringList entryOrder = dlg.order();
    if (!dlg.orderAsDefault()) {
        edited.order = old.order;
        const QStringList partIds = [&] {
            QStringList ids;
            for (const SongPart &p : std::as_const(edited.parts)) {
                ids << p.id;
            }
            return ids;
        }();
        // Parts removed in the dialog must not stay in the default order (parts of linked songs stay)
        edited.order.erase(std::remove_if(edited.order.begin(), edited.order.end(),
                                          [&](const QString &id) {
                                              return !id.contains(QLatin1Char('@')) && !partIds.contains(id);
                                          }),
                           edited.order.end());
    }
    // The entry follows the default as long as it has no own order
    if (entryOrder == edited.order) {
        o.remove("order");
    } else {
        o.insert("order", QJsonArray::fromStringList(entryOrder));
    }
    o.insert("authors", edited.authors);
    o.insert("copyright", edited.copyright);
    o.insert("ccli", edited.ccliNumber);
    // Translation: only for this event
    const QString translationId = dlg.translationId();
    if (translationId.isEmpty()) {
        o.remove("translation");
    } else if (o.value("translation").toObject().value("id").toString() != translationId) {
        o.insert("translation", QJsonObject{{"id", translationId}});   // filled by refreshSongEntry()
    }
    if (item->text() == old.title) {
        item->setText(edited.title);   // keep a title changed in the playlist
    }
    item->setData(MediaItem::SongRole, o);
    m_songs->update(edited);   // refreshes the lyrics of all entries of this song
    m_songs->setTranslations(edited.id, dlg.linkedIds());
    refreshSongEntry(item);
    if (item == ui->playlistWidget->currentItem()) {
        showEntry(item);
    }
}

QString MainWin::songCredits(const QListWidgetItem *item) const {
    const QJsonObject o = item->data(MediaItem::SongRole).toJsonObject();
    if (o.isEmpty()) {
        return {};
    }
    // Current data of the library, the copy of the entry if the song is gone
    const Song song = m_songs->song(o.value("id").toString());
    const QString authors   = song.isValid() ? song.authors : o.value("authors").toString();
    const QString copyright = song.isValid() ? song.copyright : o.value("copyright").toString();
    const QString ccli      = song.isValid() ? song.ccliNumber : o.value("ccli").toString();
    const QString licence   = settings.value("ccli/licence").toString();
    const QJsonObject translation = o.value("translation").toObject();

    // Per song: "Title – authors" and "© ... · CCLI Song # ...", the licence once at the end
    bool anyCcli = false;
    QStringList lines;
    const auto addSong = [&](const QString &title, const QString &songAuthors, const QString &songCopyright,
                             const QString &songCcli) {
        if (!songAuthors.isEmpty()) {
            lines << title + QStringLiteral(" \u2013 ") + songAuthors;
        }
        QStringList details;
        if (!songCopyright.isEmpty()) {
            details << QStringLiteral("\u00A9 %1").arg(songCopyright);
        }
        if (!songCcli.isEmpty()) {
            details << tr("CCLI Song # %1").arg(songCcli);
            anyCcli = true;
        }
        if (!details.isEmpty()) {
            lines << details.join(QStringLiteral("  \u00B7  "));
        }
    };
    addSong(item->text(), authors, copyright, ccli);
    if (!translation.value("slides").toArray().isEmpty()) {
        addSong(translation.value("title").toString(), translation.value("authors").toString(),
                translation.value("copyright").toString(), translation.value("ccli").toString());
    }
    for (const QJsonValue &v : o.value("others").toArray()) {
        const QJsonObject other = v.toObject();
        addSong(other.value("title").toString(), other.value("authors").toString(),
                other.value("copyright").toString(), other.value("ccli").toString());
    }
    if (anyCcli && !licence.isEmpty()) {
        const QString licenceText = tr("CCLI License # %1").arg(licence);
        if (lines.isEmpty()) {
            lines << licenceText;
        } else {
            lines.last() += QStringLiteral("  \u00B7  ") + licenceText;
        }
    }
    return lines.join('\n');
}

// ====== Media library page ======

void MainWin::openLibraryPage(bool pick) {
    ui->libraryBrowser->setPickMode(pick);
    ui->libraryBrowser->refresh();   // originals may have changed in the meantime
    m_libraryApplyBtn->setVisible(pick);
    showPage(ui->libraryPage);
}

QListWidgetItem *MainWin::createLibraryEntry(const QString &libraryId) {
    const LibraryEntry e = m_library->entry(libraryId);
    QListWidgetItem *item = createEntry(e.title, e.type, e.path(), {});
    item->setData(MediaItem::LibraryRole, libraryId);
    return item;
}

void MainWin::applyLibrarySelection() {
    const QStringList ids = ui->libraryBrowser->selectedIds();
    if (ids.isEmpty()) {
        return;
    }
    showPage(ui->presenterPage);
    for (const QString &id : ids) {
        insertEntry(createLibraryEntry(id));
    }
    convertPresentations();
}

void MainWin::syncLibraryPaths() {
    // Copy <-> link switches the file location; the playlist has to follow without
    // counting as a change of the event
    const bool wasLoading = m_loading;
    m_loading = true;
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        QListWidgetItem *item = ui->playlistWidget->item(i);
        const LibraryEntry e = m_library->entry(item->data(MediaItem::LibraryRole).toString());
        if (e.isValid() && item->data(MediaItem::SourceRole).toString() != e.path()) {
            item->setData(MediaItem::SourceRole, e.path());
        }
    }
    m_loading = wasLoading;
    ui->playlistWidget->viewport()->update();   // missing markers
}

// ====== Bible page ======

void MainWin::openBiblePage(QListWidgetItem *editItem) {
    ui->bibleBrowser->reloadBibles();   // bibles may have been installed in the meantime
    if (editItem) {
        ui->bibleBrowser->setPassage(BiblePassage::fromJson(editItem->data(MediaItem::BibleRole).toJsonObject()));
    } else {
        ui->bibleBrowser->startNew();
    }
    showPage(ui->biblePage);
    m_editingBibleItem = editItem;
}

void MainWin::applyBiblePassage() {
    const BiblePassage passage = ui->bibleBrowser->passage();
    if (!passage.isValid()) {
        return;
    }
    // Plain text copy: shown in the playlist and usable without the bible
    QStringList lines;
    for (int i = 0; i < passage.verses.size() && i < passage.texts.size(); ++i) {
        lines << QStringLiteral("%1 %2").arg(passage.verses.at(i)).arg(passage.texts.at(i));
    }

    QListWidgetItem *item = m_editingBibleItem;
    if (item) {
        // Keep a title the user has changed
        const BiblePassage old = BiblePassage::fromJson(item->data(MediaItem::BibleRole).toJsonObject());
        if (item->text() == old.reference()) {
            item->setText(passage.reference());
        }
        item->setData(MediaItem::TextRole, lines.join('\n'));
        item->setData(MediaItem::BibleRole, passage.toJson());
    } else {
        item = createEntry(passage.reference(), MediaItem::Bible, {}, lines.join('\n'));
        item->setData(MediaItem::BibleRole, passage.toJson());
    }

    const bool editing = m_editingBibleItem != nullptr;
    showPage(ui->presenterPage);
    if (!editing) {
        insertEntry(item);   // behind the current entry, becomes the current one
    } else if (item == ui->playlistWidget->currentItem()) {
        showEntry(item);
    }
}

void MainWin::updateEventHeader() {
    setWindowTitle(QStringLiteral("%1 %2").arg(APP_NAME, APP_VERSION));
    m_eventHeader->setEvent(m_events->event(m_eventId));
    m_eventHeader->setEntryCount(ui->playlistWidget->count());
}

void MainWin::editCurrentEvent() {
    const EventInfo e = m_events->event(m_eventId);
    if (!e.isValid()) {
        openEventsPage();
        return;
    }
    EventDialog dlg(e, {}, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_events->update(dlg.info());
    }
}

// ====== Slides ======

void MainWin::showEntry(QListWidgetItem *item) {
    const QSignalBlocker blocker(ui->slidesListWidget);
    ui->slidesListWidget->clear();
    m_deck.reset();
    m_currentSlide = -1;

    const bool startAtLast = m_startAtLastSlide;
    m_startAtLastSlide = false;

    m_beamer->unloadVideo();
    m_youTubeId.clear();
    m_youTubeThumb = QImage();
    ui->previewLabel->clear();
    ui->videoToolbar->hide();
    ui->slidesListWidget->show();

    if (!item) {
        ui->previewLabel->setText(m_eventId.isEmpty() ? tr("Create a new event or open one under \"Events\".")
                                                      : tr("Add files or text slides to the playlist."));
        updateBeamer();
        return;
    }

    const auto type = MediaItem::Type(item->data(MediaItem::TypeRole).toInt());
    if (type == MediaItem::Blank) {
        ui->slidesListWidget->hide();
        ui->previewLabel->setText(tr("Blank – the projector shows nothing"));
        updateBeamer();   // no deck -> black
        return;
    }
    if (type == MediaItem::YouTube) {
        m_youTubeId = MediaItem::youTubeId(item->data(MediaItem::SourceRole).toString());
        if (m_youTubeId.isEmpty()) {
            ui->previewLabel->setText(tr("This is not a valid YouTube link."));
            updateBeamer();
            return;
        }
        ui->slidesListWidget->hide();
        ui->videoToolbar->show();
        ui->previewLabel->setText(tr("YouTube video – plays on the projector"));
        m_beamer->setImage(QImage());
        m_beamer->loadYouTube(m_youTubeId);
        fetchYouTubeThumbnail(m_youTubeId);
        return;
    }

    if (type == MediaItem::PowerPoint) {
        const QString source = item->data(MediaItem::SourceRole).toString();
        if (PresentationConverter::cachedPdf(source).isEmpty()) {
            ui->slidesListWidget->hide();
            ui->previewLabel->setText(tr("Converting presentation..."));
            m_converter->convert(source);
            updateBeamer();
            return;
        }
    }

    QString error;
    m_deck = SlideDeck::create(MediaItem::Type(item->data(MediaItem::TypeRole).toInt()),
                               item->data(MediaItem::SourceRole).toString(),
                               item->data(MediaItem::TextRole).toString(),
                               item->data(MediaItem::BibleRole).toJsonObject(), &error,
                               songCredits(item), songTranslationSlides(item));
    if (!m_deck) {
        ui->previewLabel->setText(error);
        updateBeamer();
        return;
    }

    // Thumbnails
    const qreal dpr = devicePixelRatioF();
    // Songs: the part of every slide as label ("Vers 1", "Chorus", ...)
    QStringList labels = songSlideLabels(item);
    if (labels.size() != m_deck->count()) {
        labels.clear();   // lyrics of the entry differ from the library (copy only)
    }
    for (int i = 0; i < m_deck->count(); ++i) {
        QImage thumb = m_deck->render(i, kThumbSize * dpr);
        thumb.setDevicePixelRatio(dpr);
        QString label = QString::number(i + 1);
        if (!labels.isEmpty()) {
            label = labels.at(i);
        }
        auto *slide = new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)), label);
        slide->setToolTip(QString::number(i + 1));
        ui->slidesListWidget->addItem(slide);
    }

    const int start = startAtLast ? m_deck->count() - 1 : 0;
    ui->slidesListWidget->setCurrentRow(start);
    showSlide(start);
}

void MainWin::showSlide(int index) {
    if (!m_deck || index < 0 || index >= m_deck->count()) {
        return;
    }
    m_currentSlide = index;
    ui->slidesListWidget->scrollToItem(ui->slidesListWidget->item(index));
    updatePreview();
    updateBeamer();
}

void MainWin::nextSlide() {
    if (m_deck && m_currentSlide + 1 < m_deck->count()) {
        ui->slidesListWidget->setCurrentRow(m_currentSlide + 1);
        return;
    }
    // Last slide reached: continue with the next entry
    const int row = ui->playlistWidget->currentRow();
    if (row + 1 < ui->playlistWidget->count()) {
        ui->playlistWidget->setCurrentRow(row + 1);
    }
}

void MainWin::previousSlide() {
    if (m_deck && m_currentSlide > 0) {
        ui->slidesListWidget->setCurrentRow(m_currentSlide - 1);
        return;
    }
    const int row = ui->playlistWidget->currentRow();
    if (row > 0) {
        m_startAtLastSlide = true;
        ui->playlistWidget->setCurrentRow(row - 1);
    }
}

void MainWin::updatePreview() {
    const qreal dpr = devicePixelRatioF();
    const QSize size = ui->previewLabel->size() * dpr;

    QImage image;
    if (m_deck && m_currentSlide >= 0) {
        image = m_deck->render(m_currentSlide, size);
    } else if (!m_youTubeThumb.isNull()) {
        image = m_youTubeThumb.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        return;
    }
    image.setDevicePixelRatio(dpr);
    ui->previewLabel->setPixmap(QPixmap::fromImage(image));
}

// ====== Projector ======

void MainWin::updateBeamer() {
    if (!m_beamer->isVisible()) {
        return;
    }
    if (!m_deck || m_currentSlide < 0) {
        m_beamer->setImage(QImage());
        return;
    }
    m_beamer->setImage(m_deck->render(m_currentSlide, m_beamer->outputSize()));
}

void MainWin::setBeamerVisible(bool visible) {
    if (visible) {
        m_beamer->showOn(BeamerWindow::projectorScreen());
        // Window size is final only after the window manager has handled the show
        QTimer::singleShot(100, this, &MainWin::updateBeamer);
    } else {
        m_beamer->hide();
    }
    updateButtonStates();
}

void MainWin::setBlack(bool black) {
    m_beamer->setBlack(black);
}

// ====== Events ======

bool MainWin::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->previewLabel && event->type() == QEvent::Resize) {
        m_previewTimer.start();
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWin::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
}

void MainWin::closeEvent(QCloseEvent *event) {
    saveEvent();
    m_beamer->close();
    event->accept();
}

MainWin::~MainWin()
{
    delete m_converter;
    delete m_beamer;
    delete ui;
}
