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

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QSaveFile>
#include <QShortcut>

namespace {
constexpr QSize kThumbSize{160, 90};
const char *kPlaylistFilter = QT_TRANSLATE_NOOP("MainWin", "Event playlist (*.cwm)");
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

    initializeForm();
    initializeConnections();
    initializeShortcuts();
    updateWindowTitle();
    updateButtonStates();
}

// ====== Initializing / Settings  ======

void MainWin::initializeForm() {
    // --- Main toolbar
    m_newBtn  = ui->presenterToolbar->addButton("newBtn",  tr("New"),  ":icons/new",  true);
    m_openBtn = ui->presenterToolbar->addButton("openBtn", tr("Open"), ":icons/open", true);
    m_saveBtn = ui->presenterToolbar->addButton("saveBtn", tr("Save"), ":icons/save", true);

    ui->presenterToolbar->addSpacer();

    m_beamerBtn = ui->presenterToolbar->addButton("beamerBtn", tr("Projector"), ":icons/monitor", true);
    m_beamerBtn->setCheckable(true);
    m_beamerBtn->setToolTip(tr("Show / hide projector output (F5)"));

    m_blackBtn = ui->presenterToolbar->addButton("blackBtn", tr("Black"), ":icons/black_screen", true);
    m_blackBtn->setCheckable(true);
    m_blackBtn->setToolTip(tr("Blank the projector (B)"));

    ui->presenterToolbar->addSpacer();  // Spacer for align buttons right

    m_libraryBtn = ui->presenterToolbar->addButton("libraryBtn", "", ":icons/library");
    m_libraryBtn->setToolTip(tr("Media library"));
    m_settingsBtn = ui->presenterToolbar->addButton("settingsBtn", "", ":icons/settings");
    m_settingsBtn->setToolTip(tr("Settings"));
    m_helpBtn = ui->presenterToolbar->addButton("helpBtn", "", ":icons/help");
    m_helpBtn->setEnabled(false);

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
                       this, [this] { addTextEntry(MediaItem::Song); });
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
    ui->previewLabel->setText(tr("Add files or text slides to the playlist."));

    ui->splitter->setStretchFactor(0, 0);
    ui->splitter->setStretchFactor(1, 1);
    ui->splitter->setSizes({380, 820});

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(60);
}

void MainWin::initializeConnections() {
    connect(m_newBtn,  &QToolButton::clicked, this, &MainWin::newPlaylist);
    connect(m_settingsBtn, &QToolButton::clicked, this, [this] { showPage(ui->settingsPage); });
    connect(m_backBtn,     &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
    connect(m_bibleBackBtn,  &QToolButton::clicked, this, [this] { showPage(ui->presenterPage); });
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
    connect(m_openBtn, &QToolButton::clicked, this, &MainWin::openPlaylist);
    connect(m_saveBtn, &QToolButton::clicked, this, &MainWin::savePlaylist);
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

    // Every change of the playlist model marks the event as modified (incl. drag & drop)
    QAbstractItemModel *model = ui->playlistWidget->model();
    auto markModified = [this] { if (!m_loading) setModified(true); };
    connect(model, &QAbstractItemModel::rowsInserted, this, markModified);
    connect(model, &QAbstractItemModel::rowsRemoved,  this, markModified);
    connect(model, &QAbstractItemModel::rowsMoved,    this, markModified);
    connect(model, &QAbstractItemModel::dataChanged,  this, markModified);

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
    presenterShortcut(QKeySequence::Save, this, &MainWin::savePlaylist);
    presenterShortcut(QKeySequence::Open, this, &MainWin::openPlaylist);
    presenterShortcut(QKeySequence::New,  this, &MainWin::newPlaylist);
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
    TextSlideDialog dlg(this);
    dlg.setType(type);
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

    TextSlideDialog dlg(this);
    dlg.setType(type);
    dlg.setTitle(item->text());
    dlg.setText(item->data(MediaItem::TextRole).toString());
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    item->setText(dlg.title());
    item->setData(MediaItem::TypeRole, int(dlg.type()));
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
    setModified(true);
}

void MainWin::newPlaylist() {
    if (!maybeSave()) {
        return;
    }
    m_loading = true;
    ui->playlistWidget->clear();
    m_loading = false;
    m_playlistPath.clear();
    setModified(false);
}

void MainWin::openPlaylist() {
    if (!maybeSave()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open event"), settings.value("lastPlaylistDir").toString(), tr(kPlaylistFilter));
    if (!path.isEmpty()) {
        loadPlaylistFile(path);
    }
}

bool MainWin::savePlaylist() {
    return m_playlistPath.isEmpty() ? savePlaylistAs() : writePlaylistFile(m_playlistPath);
}

bool MainWin::savePlaylistAs() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save event"), settings.value("lastPlaylistDir").toString(), tr(kPlaylistFilter));
    if (path.isEmpty()) {
        return false;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".cwm";
    }
    return writePlaylistFile(path);
}

bool MainWin::loadPlaylistFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Open event"), tr("Cannot open %1.").arg(path));
        return false;
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();

    m_loading = true;
    ui->playlistWidget->clear();
    for (const QJsonValue &v : root.value("items").toArray()) {
        const QJsonObject o = v.toObject();
        QListWidgetItem *item = createEntry(o.value("title").toString(),
                                            MediaItem::typeFromKey(o.value("type").toString()),
                                            o.value("source").toString(),
                                            o.value("text").toString());
        if (o.contains("bible")) {
            item->setData(MediaItem::BibleRole, o.value("bible").toObject());
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
        ui->playlistWidget->addItem(item);
    }
    m_loading = false;
    convertPresentations();

    m_playlistPath = path;
    settings.setValue("lastPlaylistDir", QFileInfo(path).absolutePath());
    setModified(false);

    if (ui->playlistWidget->count() > 0) {
        ui->playlistWidget->setCurrentRow(0);
    }
    return true;
}

bool MainWin::writePlaylistFile(const QString &path) {
    QJsonArray items;
    for (int i = 0; i < ui->playlistWidget->count(); ++i) {
        const QListWidgetItem *item = ui->playlistWidget->item(i);
        const auto type = MediaItem::Type(item->data(MediaItem::TypeRole).toInt());
        QJsonObject o{{"title", item->text()}, {"type", MediaItem::typeKey(type)}};
        if (MediaItem::isTextType(type)) {
            o.insert("text", item->data(MediaItem::TextRole).toString());
            const QJsonObject bible = item->data(MediaItem::BibleRole).toJsonObject();
            if (!bible.isEmpty()) {
                o.insert("bible", bible);
            }
        } else {
            o.insert("source", item->data(MediaItem::SourceRole).toString());
            const QString libraryId = item->data(MediaItem::LibraryRole).toString();
            if (!libraryId.isEmpty()) {
                o.insert("library", libraryId);
            }
        }
        items.append(o);
    }
    const QJsonObject root{{"version", 1}, {"items", items}};

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Save event"), tr("Cannot write %1.").arg(path));
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Save event"), tr("Cannot write %1.").arg(path));
        return false;
    }

    m_playlistPath = path;
    settings.setValue("lastPlaylistDir", QFileInfo(path).absolutePath());
    setModified(false);
    return true;
}

bool MainWin::maybeSave() {
    if (!m_modified) {
        return true;
    }
    const auto answer = QMessageBox::question(
        this, tr("Unsaved changes"), tr("The event has been modified. Save changes?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (answer == QMessageBox::Save) {
        return savePlaylist();
    }
    return answer == QMessageBox::Discard;
}

void MainWin::setModified(bool modified) {
    m_modified = modified;
    updateWindowTitle();
    updateButtonStates();
}

void MainWin::updateButtonStates() {
    const int row   = ui->playlistWidget->currentRow();
    const int count = ui->playlistWidget->count();
    const bool hasEntry = row >= 0;

    m_newBtn->setEnabled(count > 0 || !m_playlistPath.isEmpty());
    m_saveBtn->setEnabled(m_modified);
    m_blackBtn->setEnabled(m_beamerBtn->isChecked());

    m_editBtn->setEnabled(hasEntry);
    m_removeBtn->setEnabled(hasEntry);
    m_upBtn->setEnabled(row > 0);
    m_downBtn->setEnabled(hasEntry && row < count - 1);

    m_bibleApplyBtn->setEnabled(ui->bibleBrowser->passage().isValid());

    const bool librarySelection = !ui->libraryBrowser->selectedIds().isEmpty();
    m_libraryUpdateBtn->setEnabled(ui->libraryBrowser->canUpdateSelection());
    m_libraryRemoveBtn->setEnabled(librarySelection);
    m_libraryApplyBtn->setEnabled(librarySelection);
}

void MainWin::showPage(QWidget *page) {
    ui->mainStack->setCurrentWidget(page);
    const bool presenter = page == ui->presenterPage;
    for (QShortcut *shortcut : m_presenterShortcuts) {
        shortcut->setEnabled(presenter);
    }
    if (page != ui->biblePage) {
        m_editingBibleItem = nullptr;
    }
    updateButtonStates();
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

void MainWin::updateWindowTitle() {
    const QString event = m_playlistPath.isEmpty() ? tr("New event")
                                                   : QFileInfo(m_playlistPath).completeBaseName();
    setWindowTitle(QStringLiteral("%1%2 – %3 %4").arg(event, m_modified ? "*" : "",
                                                      APP_NAME, APP_VERSION));
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
        ui->previewLabel->setText(tr("Add files or text slides to the playlist."));
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
                               item->data(MediaItem::BibleRole).toJsonObject(), &error);
    if (!m_deck) {
        ui->previewLabel->setText(error);
        updateBeamer();
        return;
    }

    // Thumbnails
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < m_deck->count(); ++i) {
        QImage thumb = m_deck->render(i, kThumbSize * dpr);
        thumb.setDevicePixelRatio(dpr);
        ui->slidesListWidget->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(thumb)),
                                                          QString::number(i + 1)));
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
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    m_beamer->close();
    event->accept();
}

MainWin::~MainWin()
{
    delete m_converter;
    delete m_beamer;
    delete ui;
}
