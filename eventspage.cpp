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
#include "eventspage.h"
#include "eventdialog.h"
#include "eventstore.h"
#include "searchfield.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kIdRole = Qt::UserRole + 1;

enum Column { ColName, ColDate, ColTime, ColEntries, ColNote };

const char *kEventFilter = QT_TRANSLATE_NOOP("EventsPage", "Event (*.cwm)");

} // namespace

EventsPage::EventsPage(QWidget *parent)
    : QWidget(parent)
{
    m_search = new SearchField(this);

    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Name"), tr("Date"), tr("Time"), tr("Entries"), tr("Note")});
    m_tree->setRootIsDecorated(false);
    m_tree->setItemsExpandable(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    // Same look as the bible table in the settings: more air than the default table look.
    // The header only gets more height, a style sheet on it would replace the native look.
    m_tree->setStyleSheet(QStringLiteral("QTreeView::item { padding: 8px 10px; }"));
    m_tree->header()->setMinimumHeight(34);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(ColName, 280);
    m_tree->setColumnWidth(ColDate, 220);
    m_tree->setColumnWidth(ColTime, 70);
    m_tree->setColumnWidth(ColEntries, 70);

    m_status = new QLabel(this);
    m_status->setEnabled(false);   // muted

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(m_search);
    layout->addWidget(m_tree, 1);
    layout->addWidget(m_status);

    connect(m_search, &QLineEdit::textChanged, this, &EventsPage::applyFilter);
    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &EventsPage::selectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, &EventsPage::showContextMenu);
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        const QString id = item->data(ColName, kIdRole).toString();
        if (!id.isEmpty()) {
            emit openRequested(id);
        }
    });
}

void EventsPage::setStore(EventStore *store)
{
    m_store = store;
    connect(m_store, &EventStore::changed, this, &EventsPage::refresh);
    refresh();
}

void EventsPage::setCurrentId(const QString &id)
{
    m_currentId = id;
    refresh();
}

QString EventsPage::selectedId() const
{
    const QList<QTreeWidgetItem *> items = m_tree->selectedItems();
    return items.isEmpty() ? QString() : items.first()->data(ColName, kIdRole).toString();
}

// ---------------------------------------------------------------------------
// List
// ---------------------------------------------------------------------------

QTreeWidgetItem *EventsPage::addGroup(const QString &title)
{
    auto *group = new QTreeWidgetItem(m_tree, {title});
    group->setFlags(Qt::ItemIsEnabled);   // not selectable
    group->setFirstColumnSpanned(true);
    QFont font = m_tree->font();
    font.setBold(true);
    group->setFont(ColName, font);
    group->setExpanded(true);
    return group;
}

void EventsPage::addEvent(QTreeWidgetItem *group, const EventInfo &e)
{
    const QLocale locale;
    auto *item = new QTreeWidgetItem(group);
    item->setData(ColName, kIdRole, e.id);
    item->setText(ColName, e.name);
    if (!e.isTemplate) {
        item->setText(ColDate, locale.toString(e.date, QLocale::LongFormat));
        item->setText(ColTime, locale.toString(e.time, QLocale::ShortFormat));
    }
    item->setText(ColEntries, QString::number(e.itemCount));
    item->setTextAlignment(ColEntries, Qt::AlignRight | Qt::AlignVCenter);
    item->setText(ColNote, e.note.section('\n', 0, 0));
    if (!e.note.isEmpty()) {
        item->setToolTip(ColNote, e.note);
    }
    if (e.id == m_currentId) {
        QFont font = m_tree->font();
        font.setBold(true);
        for (int c = 0; c < m_tree->columnCount(); ++c) {
            item->setFont(c, font);
        }
        item->setToolTip(ColName, tr("Currently open"));
    }
}

void EventsPage::refresh()
{
    if (!m_store) {
        return;
    }
    const QString keep = selectedId();

    const QSignalBlocker blocker(m_tree);
    m_tree->clear();

    QList<EventInfo> upcoming, past;
    for (const EventInfo &e : m_store->events()) {   // newest first
        (e.isPast() ? past : upcoming) << e;
    }
    std::reverse(upcoming.begin(), upcoming.end());   // the next event on top

    QTreeWidgetItem *group = addGroup(tr("Upcoming"));
    for (const EventInfo &e : std::as_const(upcoming)) {
        addEvent(group, e);
    }
    group = addGroup(tr("Past"));
    for (const EventInfo &e : std::as_const(past)) {
        addEvent(group, e);
    }
    group = addGroup(tr("Templates"));
    for (const EventInfo &e : m_store->templates()) {
        addEvent(group, e);
    }

    selectId(keep);
    applyFilter();
    emit selectionChanged();
}

void EventsPage::applyFilter()
{
    const QString text = m_search->text().trimmed();
    int visible = 0, total = 0;
    for (int g = 0; g < m_tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = m_tree->topLevelItem(g);
        int groupVisible = 0;
        for (int i = 0; i < group->childCount(); ++i) {
            QTreeWidgetItem *item = group->child(i);
            const bool show = text.isEmpty() || item->text(ColName).contains(text, Qt::CaseInsensitive)
                              || item->toolTip(ColNote).contains(text, Qt::CaseInsensitive);
            item->setHidden(!show);
            groupVisible += show ? 1 : 0;
        }
        group->setHidden(groupVisible == 0);   // no empty headings
        visible += groupVisible;
        total += group->childCount();
    }
    m_status->setText(total == 0
        ? tr("There are no events yet. Create one with \"New\".")
        : (visible == total ? tr("%n entries", "", total)
                            : tr("%1 of %n entries", "", total).arg(visible)));
}

void EventsPage::selectId(const QString &id)
{
    if (id.isEmpty()) {
        return;
    }
    for (int g = 0; g < m_tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *group = m_tree->topLevelItem(g);
        for (int i = 0; i < group->childCount(); ++i) {
            if (group->child(i)->data(ColName, kIdRole).toString() == id) {
                m_tree->setCurrentItem(group->child(i));
                m_tree->scrollToItem(group->child(i));
                return;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void EventsPage::newEvent()
{
    EventDialog dlg(EventInfo(), m_store->templates(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString templateId = dlg.templateId();
    const QString id = m_store->create(dlg.info(), templateId.isEmpty() ? QJsonArray()
                                                                        : m_store->items(templateId));
    if (!id.isEmpty()) {
        emit openRequested(id);
    }
}

void EventsPage::newTemplate()
{
    EventInfo info;
    info.isTemplate = true;
    EventDialog dlg(info, {}, this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const QString id = m_store->create(dlg.info());
    if (!id.isEmpty()) {
        emit openRequested(id);   // an empty template is only useful after filling it
    }
}

void EventsPage::editSelected()
{
    const EventInfo e = m_store->event(selectedId());
    if (!e.isValid()) {
        return;
    }
    EventDialog dlg(e, {}, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_store->update(dlg.info());
    }
}

void EventsPage::removeSelected()
{
    const EventInfo e = m_store->event(selectedId());
    if (!e.isValid()) {
        return;
    }
    const QString question = e.isTemplate
        ? tr("Delete the template \"%1\"?").arg(e.name)
        : tr("Delete the event \"%1\" (%2)?").arg(e.name, QLocale().toString(e.date, QLocale::ShortFormat));
    if (QMessageBox::question(this, tr("Delete"),
                              question + "\n\n" + tr("The files of the media library are not deleted."))
        == QMessageBox::Yes) {
        m_store->remove(e.id);
    }
}

void EventsPage::saveSelectedAsTemplate()
{
    const EventInfo e = m_store->event(selectedId());
    if (!e.isValid() || e.isTemplate) {
        return;
    }
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Save as template"), tr("Name of the template:"),
                                               QLineEdit::Normal, e.name, &ok).trimmed();
    if (!ok || name.isEmpty()) {
        return;
    }
    EventInfo t;
    t.name = name;
    t.note = e.note;
    t.isTemplate = true;
    selectId(m_store->create(t, m_store->items(e.id)));
}

void EventsPage::importFiles()
{
    QSettings settings;
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Import events"), settings.value("lastEventDir").toString(), tr(kEventFilter));
    if (files.isEmpty()) {
        return;
    }
    settings.setValue("lastEventDir", QFileInfo(files.first()).absolutePath());

    QString lastId;
    QStringList errors;
    for (const QString &file : files) {
        QString error;
        const QString id = m_store->importFile(file, &error);
        if (id.isEmpty()) {
            errors << error;
        } else {
            lastId = id;
        }
    }
    m_search->clear();
    selectId(lastId);
    if (!errors.isEmpty()) {
        QMessageBox::warning(this, tr("Import events"), errors.join('\n'));
    }
}

void EventsPage::exportSelected()
{
    const EventInfo e = m_store->event(selectedId());
    if (!e.isValid()) {
        return;
    }
    QSettings settings;
    QString name = e.isTemplate ? e.name : e.date.toString(Qt::ISODate) + " " + e.name;
    name.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")), QStringLiteral("_"));
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export event"), settings.value("lastEventDir").toString() + "/" + name + ".cwm",
        tr(kEventFilter));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".cwm";
    }
    settings.setValue("lastEventDir", QFileInfo(path).absolutePath());

    QString error;
    if (!m_store->exportFile(e.id, path, &error)) {
        QMessageBox::warning(this, tr("Export event"), error);
    }
}

void EventsPage::showContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item || item->data(ColName, kIdRole).toString().isEmpty()) {
        return;
    }
    m_tree->setCurrentItem(item);
    const EventInfo e = m_store->event(selectedId());

    QMenu menu(this);
    menu.addAction(tr("Open"), this, [this, e] { emit openRequested(e.id); });
    menu.addAction(tr("Properties..."), this, &EventsPage::editSelected);
    menu.addSeparator();
    if (!e.isTemplate) {
        menu.addAction(tr("Save as template..."), this, &EventsPage::saveSelectedAsTemplate);
    }
    menu.addAction(tr("Export..."), this, &EventsPage::exportSelected);
    menu.addSeparator();
    menu.addAction(tr("Delete"), this, &EventsPage::removeSelected);
    menu.exec(m_tree->viewport()->mapToGlobal(pos));
}
