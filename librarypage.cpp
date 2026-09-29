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
#include "librarypage.h"
#include "medialibrary.h"

#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QSettings>
#include <QSet>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kIdRole    = Qt::UserRole + 1;
constexpr int kInfoRole  = Qt::UserRole + 2;   // second line
constexpr int kStateRole = Qt::UserRole + 3;   // State

enum State { StateOk, StateUpdate, StateMissing };

constexpr QSize kGrid{232, 190};

// Tile: thumbnail (16:9), title, info line (type · copy/link · state)
class LibraryDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        const QIcon icon = opt.icon;
        opt.text.clear();
        opt.icon = QIcon();
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        painter->save();
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor textColor = opt.palette.color(QPalette::Normal, selected ? QPalette::HighlightedText
                                                                              : QPalette::Text);

        // Thumbnail on a dark 16:9 area, like a slide
        const QRect thumb = thumbRect(opt.rect);
        painter->fillRect(thumb, QColor(0, 0, 0, 160));
        icon.paint(painter, thumb.adjusted(2, 2, -2, -2), Qt::AlignCenter);

        // Title
        QFont titleFont = opt.font;
        titleFont.setBold(true);
        const QFontMetrics fmTitle(titleFont);
        const QRect title = titleRect(opt.rect, fmTitle);
        painter->setFont(titleFont);
        painter->setPen(textColor);
        painter->drawText(title, Qt::AlignLeft | Qt::AlignVCenter,
                          fmTitle.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideRight, title.width()));

        // Info line, colored by state
        const int state = index.data(kStateRole).toInt();
        QColor infoColor = textColor;
        infoColor.setAlphaF(0.65);
        if (state == StateMissing) infoColor = QColor(220, 80, 70);
        if (state == StateUpdate)  infoColor = QColor(225, 150, 40);
        const QFontMetrics fm(opt.font);
        const QRect info(title.left(), title.bottom() + 1, title.width(), fm.height());
        painter->setFont(opt.font);
        painter->setPen(infoColor);
        painter->drawText(info, Qt::AlignLeft | Qt::AlignVCenter,
                          fm.elidedText(index.data(kInfoRole).toString(), Qt::ElideRight, info.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return kGrid - QSize(8, 8);
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &) const override
    {
        QFont f = option.font;
        f.setBold(true);
        editor->setGeometry(titleRect(option.rect, QFontMetrics(f)));
    }

private:
    static QRect thumbRect(const QRect &cell)
    {
        const QRect r = cell.adjusted(8, 8, -8, -8);
        return QRect(r.left(), r.top(), r.width(), r.width() * 9 / 16);
    }
    static QRect titleRect(const QRect &cell, const QFontMetrics &fm)
    {
        const QRect t = thumbRect(cell);
        return QRect(t.left(), t.bottom() + 6, t.width(), fm.height() + 2);
    }
};

} // namespace

LibraryPage::LibraryPage(QWidget *parent)
    : QWidget(parent)
{
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search..."));
    m_search->setClearButtonEnabled(true);

    m_typeFilter = new QComboBox(this);
    m_typeFilter->addItem(tr("All types"), int(MediaItem::Unknown));
    for (MediaItem::Type t : {MediaItem::Image, MediaItem::Pdf, MediaItem::PowerPoint}) {
        m_typeFilter->addItem(MediaItem::typeIcon(t), MediaItem::typeName(t), int(t));
    }

    m_list = new QListWidget(this);
    m_list->setViewMode(QListView::IconMode);
    m_list->setMovement(QListView::Static);
    m_list->setResizeMode(QListView::Adjust);
    m_list->setWrapping(true);
    m_list->setUniformItemSizes(true);
    m_list->setGridSize(kGrid);
    m_list->setIconSize(QSize(320, 180));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setEditTriggers(QAbstractItemView::EditKeyPressed);   // F2 renames
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setItemDelegate(new LibraryDelegate(m_list));

    m_status = new QLabel(this);
    m_status->setEnabled(false);   // muted

    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(m_search, 1);
    filterRow->addWidget(m_typeFilter);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->addLayout(filterRow);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_status);

    connect(m_search, &QLineEdit::textChanged, this, &LibraryPage::applyFilter);
    connect(m_typeFilter, &QComboBox::currentIndexChanged, this, &LibraryPage::applyFilter);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &LibraryPage::selectionChanged);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &LibraryPage::showContextMenu);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (m_pickMode) {
            emit pickRequested();
        } else {
            m_list->editItem(item);
        }
    });
    // Renamed in place
    connect(m_list, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (m_library) {
            m_library->rename(item->data(kIdRole).toString(), item->text());
        }
    });
}

void LibraryPage::setLibrary(MediaLibrary *library)
{
    m_library = library;
    connect(m_library, &MediaLibrary::changed, this, &LibraryPage::refresh);
    connect(m_library, &MediaLibrary::thumbnailChanged, this, [this](const QString &id) {
        for (int i = 0; i < m_list->count(); ++i) {
            if (m_list->item(i)->data(kIdRole).toString() == id) {
                updateItem(m_list->item(i));
            }
        }
    });
    refresh();
}

void LibraryPage::setPickMode(bool pick)
{
    m_pickMode = pick;
    m_list->clearSelection();
    m_search->clear();
}

QStringList LibraryPage::selectedIds() const
{
    QStringList ids;
    // In display order, so several entries end up in the playlist in the same order
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->isSelected() && !m_list->item(i)->isHidden()) {
            ids << m_list->item(i)->data(kIdRole).toString();
        }
    }
    return ids;
}

bool LibraryPage::canUpdateSelection() const
{
    if (!m_library) {
        return false;
    }
    for (const QString &id : selectedIds()) {
        if (m_library->entry(id).updateAvailable()) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

void LibraryPage::refresh()
{
    if (!m_library) {
        return;
    }
    const QStringList keep = selectedIds();

    const QSignalBlocker blocker(m_list);   // no rename/selection signals while rebuilding
    m_list->clear();
    for (const LibraryEntry &e : m_library->entries()) {
        auto *item = new QListWidgetItem(e.title);
        item->setData(kIdRole, e.id);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_list->addItem(item);
        updateItem(item);
    }
    selectIds(keep);
    applyFilter();
    emit selectionChanged();
}

void LibraryPage::updateItem(QListWidgetItem *item)
{
    const LibraryEntry e = m_library->entry(item->data(kIdRole).toString());
    const QSignalBlocker blocker(m_list);

    const QImage thumb = m_library->thumbnail(e.id);
    item->setIcon(thumb.isNull() ? MediaItem::typeIcon(e.type) : QIcon(QPixmap::fromImage(thumb)));

    QStringList info{MediaItem::typeName(e.type)};
    info << (e.linked ? tr("🔗 linked") : tr("copy"));
    State state = StateOk;
    if (e.isMissing()) {
        state = StateMissing;
        info << tr("file missing");
    } else if (e.updateAvailable()) {
        state = StateUpdate;
        info << tr("⟳ newer version");
    }
    item->setData(kInfoRole, info.join(QStringLiteral(" · ")));
    item->setData(kStateRole, state);

    QString tooltip = QStringLiteral("<b>%1</b><br>%2").arg(e.title.toHtmlEscaped(), info.join(QStringLiteral(" · ")));
    tooltip += "<br>" + (e.linked ? tr("File: %1") : tr("Original: %1")).arg(e.originalPath.toHtmlEscaped());
    item->setToolTip(tooltip);
}

void LibraryPage::applyFilter()
{
    const QString text = m_search->text().trimmed();
    const auto type = MediaItem::Type(m_typeFilter->currentData().toInt());
    int visible = 0;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const LibraryEntry e = m_library ? m_library->entry(item->data(kIdRole).toString()) : LibraryEntry();
        const bool show = (text.isEmpty() || e.title.contains(text, Qt::CaseInsensitive))
                          && (type == MediaItem::Unknown || e.type == type);
        item->setHidden(!show);
        visible += show ? 1 : 0;
    }
    const int total = m_list->count();
    m_status->setText(total == 0
        ? tr("The media library is empty. Add images, PDFs or presentations with \"Add\".")
        : (visible == total ? tr("%n entries", "", total)
                            : tr("%1 of %n entries", "", total).arg(visible)));
}

void LibraryPage::selectIds(const QStringList &ids)
{
    const QSet<QString> set(ids.cbegin(), ids.cend());
    QListWidgetItem *first = nullptr;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        const bool select = set.contains(item->data(kIdRole).toString());
        item->setSelected(select);
        if (select && !first) first = item;
    }
    if (first) {
        m_list->scrollToItem(first);
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void LibraryPage::addFiles()
{
    QSettings settings;
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add to media library"), settings.value("lastMediaDir").toString(),
        MediaItem::mediaFileFilter());
    if (files.isEmpty()) {
        return;
    }
    settings.setValue("lastMediaDir", QFileInfo(files.first()).absolutePath());
    const bool link = settings.value("library/linkByDefault", false).toBool();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QStringList added, errors;
    for (const QString &file : files) {
        QString error;
        const QString id = m_library->addFile(file, link, &error);
        if (id.isEmpty()) {
            errors << error;
        } else {
            added << id;
        }
    }
    QApplication::restoreOverrideCursor();

    // New entries are selected: in pick mode they can be applied right away
    m_search->clear();
    m_typeFilter->setCurrentIndex(0);
    selectIds(added);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Media library"), errors.join('\n'));
    }
}

void LibraryPage::updateSelected()
{
    QStringList errors;
    for (const QString &id : selectedIds()) {
        QString error;
        if (m_library->entry(id).updateAvailable() && !m_library->updateFromOriginal(id, &error)) {
            errors << error;
        }
    }
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Media library"), errors.join('\n'));
    }
}

void LibraryPage::removeSelected()
{
    const QStringList ids = selectedIds();
    if (ids.isEmpty()) {
        return;
    }
    const QString question = ids.size() == 1
        ? tr("Remove \"%1\" from the media library?").arg(m_library->entry(ids.first()).title)
        : tr("Remove %n entries from the media library?", "", int(ids.size()));
    if (QMessageBox::question(this, tr("Media library"),
                              question + "\n\n" + tr("Linked originals are not deleted.")) != QMessageBox::Yes) {
        return;
    }
    for (const QString &id : ids) {
        m_library->remove(id);
    }
}

void LibraryPage::setLinkedSelected(bool linked)
{
    QStringList errors;
    for (const QString &id : selectedIds()) {
        QString error;
        if (!m_library->setLinked(id, linked, &error)) {
            errors << error;
        }
    }
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Media library"), errors.join('\n'));
    }
}

void LibraryPage::showContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_list->itemAt(pos);
    if (!item) {
        return;
    }
    if (!item->isSelected()) {
        m_list->clearSelection();
        item->setSelected(true);
    }
    const QStringList ids = selectedIds();
    bool anyCopy = false, anyLink = false, anyUpdate = false;
    for (const QString &id : ids) {
        const LibraryEntry e = m_library->entry(id);
        anyCopy = anyCopy || !e.linked;
        anyLink = anyLink || e.linked;
        anyUpdate = anyUpdate || e.updateAvailable();
    }

    QMenu menu(this);
    QAction *rename = menu.addAction(tr("Rename"), this, [this, item] { m_list->editItem(item); });
    rename->setEnabled(ids.size() == 1);
    menu.addAction(tr("Update from original"), this, &LibraryPage::updateSelected)->setEnabled(anyUpdate);
    menu.addSeparator();
    menu.addAction(tr("Copy into the media library"), this, [this] { setLinkedSelected(false); })
        ->setEnabled(anyLink);
    menu.addAction(tr("Link only (keep the file where it is)"), this, [this] { setLinkedSelected(true); })
        ->setEnabled(anyCopy);
    menu.addSeparator();
    const LibraryEntry current = m_library->entry(item->data(kIdRole).toString());
    menu.addAction(tr("Show in folder"), this, [current] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(current.path()).absolutePath()));
    })->setEnabled(!current.isMissing());
    menu.addSeparator();
    menu.addAction(tr("Remove"), this, &LibraryPage::removeSelected);
    menu.exec(m_list->viewport()->mapToGlobal(pos));
}
