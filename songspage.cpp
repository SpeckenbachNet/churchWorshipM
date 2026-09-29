/*
 * Copyright (C) 2026 Michael Speckenbach
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "songspage.h"
#include "searchfield.h"
#include "songeditor.h"
#include "songstore.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kIdRole   = Qt::UserRole + 1;
constexpr int kTextRole = Qt::UserRole + 2;   // lyrics for the full text search

enum Column { ColTitle, ColAuthors, ColOrder, ColCcli };

const char *kLyricsFilter = QT_TRANSLATE_NOOP("SongsPage", "SongSelect lyrics (*.txt)");

} // namespace

SongsPage::SongsPage(QWidget *parent)
    : QWidget(parent)
{
    m_search = new SearchField(this);
    m_search->setPlaceholderText(tr("Search title, author, CCLI number or lyrics..."));

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Title"), tr("Authors"), tr("Order"), tr("CCLI")});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(ColTitle, Qt::AscendingOrder);
    // Same look as the event list and the bible table
    m_tree->setStyleSheet(QStringLiteral("QTreeView::item { padding: 8px 10px; }"));
    m_tree->header()->setMinimumHeight(34);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setColumnWidth(ColTitle, 280);
    m_tree->setColumnWidth(ColAuthors, 300);
    m_tree->setColumnWidth(ColOrder, 220);

    m_status = new QLabel(this);
    m_status->setEnabled(false);   // muted

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(m_search);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_status);

    setAcceptDrops(true);

    connect(m_search, &QLineEdit::textChanged, this, &SongsPage::applyFilter);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &SongsPage::selectionChanged);
    connect(m_tree, &QTreeWidget::itemActivated, this, [this] {
        if (m_pick) {
            emit pickRequested();
        } else {
            editSelected();
        }
    });
}

void SongsPage::setStore(SongStore *store)
{
    m_store = store;
    connect(m_store, &SongStore::changed, this, &SongsPage::refresh);
    refresh();
}

void SongsPage::setPickMode(bool pick)
{
    m_pick = pick;
    m_search->setFocus();
}

QStringList SongsPage::selectedIds() const
{
    QStringList ids;
    for (const QTreeWidgetItem *item : m_tree->selectedItems()) {
        if (!item->isHidden()) {
            ids << item->data(ColTitle, kIdRole).toString();
        }
    }
    return ids;
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

void SongsPage::refresh()
{
    if (!m_store) {
        return;
    }
    const QStringList keep = selectedIds();

    const QSignalBlocker blocker(m_tree);
    m_tree->clear();
    for (const Song &song : m_store->songs()) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setData(ColTitle, kIdRole, song.id);
        item->setData(ColTitle, kTextRole, song.allText());
        item->setText(ColTitle, song.title);
        item->setText(ColAuthors, song.authors);
        item->setToolTip(ColAuthors, song.authors);
        item->setText(ColOrder, song.order.join(QLatin1Char(' ')));
        item->setText(ColCcli, song.ccliNumber);
        if (!song.copyright.isEmpty()) {
            item->setToolTip(ColTitle, QStringLiteral("© %1").arg(song.copyright));
        }
    }

    selectIds(keep);
    applyFilter();
    emit selectionChanged();
}

void SongsPage::applyFilter()
{
    const QString text = m_search->text().trimmed();
    int visible = 0;
    const int total = m_tree->topLevelItemCount();
    for (int i = 0; i < total; ++i) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        const bool show = text.isEmpty()
                          || item->text(ColTitle).contains(text, Qt::CaseInsensitive)
                          || item->text(ColAuthors).contains(text, Qt::CaseInsensitive)
                          || item->text(ColCcli).contains(text)
                          || item->data(ColTitle, kTextRole).toString().contains(text, Qt::CaseInsensitive);
        item->setHidden(!show);
        visible += show ? 1 : 0;
    }
    m_status->setText(total == 0
        ? tr("There are no songs yet. Import SongSelect lyrics files with \"Import\" or drop them here.")
        : (visible == total ? tr("%n songs", "", total)
                            : tr("%1 of %n songs", "", total).arg(visible)));
    emit selectionChanged();
}

void SongsPage::selectIds(const QStringList &ids)
{
    QTreeWidgetItem *first = nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        if (ids.contains(item->data(ColTitle, kIdRole).toString())) {
            item->setSelected(true);
            if (!first) {
                first = item;
            }
        }
    }
    if (first) {
        m_tree->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
        m_tree->scrollToItem(first);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void SongsPage::importFiles()
{
    QSettings settings;
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Import songs"), settings.value("lastSongDir").toString(), tr(kLyricsFilter));
    if (files.isEmpty()) {
        return;
    }
    settings.setValue("lastSongDir", QFileInfo(files.first()).absolutePath());
    importPaths(files);
}

void SongsPage::importPaths(const QStringList &paths)
{
    QSettings settings;
    QStringList imported;
    QStringList errors;
    for (const QString &path : paths) {
        QString licence, error;
        const QString id = m_store->importSongSelect(path, &licence, &error);
        if (id.isEmpty()) {
            errors << error;
            continue;
        }
        imported << id;
        // The church licence is printed on every download: remember it for the song slides
        if (!licence.isEmpty() && settings.value("ccli/licence").toString().isEmpty()) {
            settings.setValue("ccli/licence", licence);
        }
    }
    m_search->clear();
    m_tree->clearSelection();
    selectIds(imported);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Import songs"), errors.join('\n'));
    }
}

void SongsPage::removeSelected()
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) {
        return;
    }
    const QString question = ids.size() == 1
        ? tr("Delete the song \"%1\" from the song library?").arg(m_store->song(ids.first()).title)
        : tr("Delete %n songs from the song library?", "", int(ids.size()));
    if (QMessageBox::question(this, tr("Delete"),
                              question + "\n\n" + tr("Events keep their copy of the lyrics."))
        != QMessageBox::Yes) {
        return;
    }
    for (const QString &id : ids) {
        m_store->remove(id);
    }
}

void SongsPage::newSong()
{
    SongEditorDialog dlg(Song(), {}, SongEditorDialog::Library, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString id = m_store->add(dlg.song());
    m_search->clear();
    m_tree->clearSelection();
    selectIds({id});
}

void SongsPage::editSelected()
{
    const QStringList ids = selectedIds();
    const Song song = ids.isEmpty() ? Song() : m_store->song(ids.first());
    if (!song.isValid()) {
        return;
    }
    SongEditorDialog dlg(song, {}, SongEditorDialog::Library, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_store->update(dlg.song());
    }
}

void SongsPage::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void SongsPage::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            paths << url.toLocalFile();
        }
    }
    if (!paths.isEmpty()) {
        event->acceptProposedAction();
        importPaths(paths);
    }
}
