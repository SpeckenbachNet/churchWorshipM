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
#pragma once

#include <QWidget>

class EventStore;
struct EventInfo;
class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

// Page of the main window's stacked widget listing all events and templates:
//   Upcoming (earliest first), Past (newest first), Templates (by name).
// The toolbar with the buttons belongs to the main window (like on the other pages).
class EventsPage : public QWidget {
    Q_OBJECT

public:
    explicit EventsPage(QWidget *parent = nullptr);

    void setStore(EventStore *store);
    void setCurrentId(const QString &id);   // the event open in the presenter (shown bold)

    QString selectedId() const;
    void    refresh();

    // Toolbar actions. New events / templates are opened right away (openRequested).
    void newEvent();
    void newTemplate();
    void editSelected();
    void removeSelected();
    void saveSelectedAsTemplate();
    void importFiles();
    void exportSelected();

signals:
    void selectionChanged();
    void openRequested(const QString &id);

private:
    void applyFilter();
    void selectId(const QString &id);
    void showContextMenu(const QPoint &pos);
    QTreeWidgetItem *addGroup(const QString &title);
    void addEvent(QTreeWidgetItem *group, const EventInfo &e);

    EventStore  *m_store = nullptr;
    QString      m_currentId;

    QLineEdit   *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel      *m_status = nullptr;
};
