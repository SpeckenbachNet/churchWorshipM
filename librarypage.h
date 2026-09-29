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

class MediaLibrary;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;

// Page of the main window's stacked widget showing the media library as tiles.
//   Manage mode: add, rename, update, remove, switch copy/link
//   Pick mode:   the same, plus choosing entries for the playlist ("Apply" in the toolbar)
// The toolbar with the buttons belongs to the main window (like on the other pages).
class LibraryPage : public QWidget {
    Q_OBJECT

public:
    explicit LibraryPage(QWidget *parent = nullptr);

    void setLibrary(MediaLibrary *library);
    void setPickMode(bool pick);
    bool isPickMode() const { return m_pickMode; }

    QStringList selectedIds() const;
    bool canUpdateSelection() const;   // a selected copy has a newer original

    void refresh();

    // Toolbar actions
    void addFiles();
    void updateSelected();
    void removeSelected();

signals:
    void selectionChanged();
    void pickRequested();   // double click in pick mode = take this entry

private:
    void applyFilter();
    void updateItem(QListWidgetItem *item);
    void showContextMenu(const QPoint &pos);
    void setLinkedSelected(bool linked);
    void selectIds(const QStringList &ids);

    MediaLibrary *m_library = nullptr;
    bool          m_pickMode = false;

    QLineEdit   *m_search = nullptr;
    QComboBox   *m_typeFilter = nullptr;
    QListWidget *m_list = nullptr;
    QLabel      *m_status = nullptr;
};
