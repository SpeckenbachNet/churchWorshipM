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

#include <QStringList>
#include <QWidget>

class SongStore;
class QLabel;
class QLineEdit;
class QTreeWidget;

// Page of the main window's stacked widget with the song library.
// Search in title, authors, CCLI number and lyrics. SongSelect lyrics files can be
// imported with the toolbar button or dropped onto the page.
// The toolbar with the buttons belongs to the main window (like on the other pages).
class SongsPage : public QWidget {
    Q_OBJECT

public:
    explicit SongsPage(QWidget *parent = nullptr);

    void setStore(SongStore *store);
    void setPickMode(bool pick);   // choosing songs for the playlist: double click applies

    QStringList selectedIds() const;
    void        refresh();

    // Toolbar actions
    void importFiles();
    void importPaths(const QStringList &paths);
    void removeSelected();

signals:
    void selectionChanged();
    void pickRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void applyFilter();
    void selectIds(const QStringList &ids);

    SongStore   *m_store = nullptr;
    bool         m_pick = false;

    QLineEdit   *m_search = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel      *m_status = nullptr;
};
