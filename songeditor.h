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

#include <QDialog>

#include "slidebackground.h"
#include "song.h"

class BackgroundPicker;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class RichTextEdit;
class QPushButton;
class SongStore;

// Edits a song: data, parts and the order in which the parts are sung.
//   Library:  everything is saved in the song library.
//   Event:    lyrics and data are saved in the library (a correction is wanted everywhere),
//             the order only applies to this event unless "use as default" is checked.
//             Linked songs can be shown as translation below the lyrics (this event only).
// The song's own background is chosen in the library (an entry's one via the playlist).
// Links to translations are saved in the library by the caller (linkedIds()).
class SongEditorDialog : public QDialog {
    Q_OBJECT

public:
    enum Mode { Library, Event };

    // 'order': order to edit (event: the entry's own order, empty = library default)
    SongEditorDialog(const Song &song, const QStringList &order, Mode mode, const SongStore *store,
                     QWidget *parent = nullptr);

    Song        song() const;     // edited song; 'order' = edited order
    QStringList order() const;    // the edited order
    bool        orderAsDefault() const;   // Library mode: always true
    QStringList linkedIds() const { return m_linked; }   // translations of the song

    // Event mode: translation shown below the lyrics, empty for none
    void        setTranslationId(const QString &id);
    QString     translationId() const;

private:
    void loadPart(int row);
    void storePart();
    void addPart(SongPart::Kind kind);
    void removePart();
    void appendToOrder(const QString &partId);
    void resetOrder();
    void refreshPartList();
    void updateOrderItem(QListWidgetItem *item) const;
    void updateButtons();
    void linkSong();
    // Part of the order: own part or "V3@<song id>" of a linked song; invalid (empty id) if unknown
    SongPart resolvePart(const QString &orderId, Song *source = nullptr) const;
    void updateTranslations();

    Song             m_song;
    Mode             m_mode;
    const SongStore *m_store = nullptr;
    QStringList      m_linked;   // ids of the linked songs
    int              m_currentPart = -1;

    QLineEdit       *m_title = nullptr;
    QLineEdit       *m_authors = nullptr;
    QLineEdit       *m_copyright = nullptr;
    QLineEdit       *m_ccli = nullptr;
    QComboBox       *m_language = nullptr;
    QLabel          *m_linkedLabel = nullptr;
    QPushButton     *m_unlinkBtn = nullptr;
    QLabel          *m_missingLabel = nullptr;
    QWidget         *m_translationRow = nullptr;
    QCheckBox       *m_showTranslation = nullptr;
    QComboBox       *m_translation = nullptr;
    QListWidget     *m_parts = nullptr;
    QPushButton     *m_addPartBtn = nullptr;
    QPushButton     *m_removePartBtn = nullptr;
    QPushButton     *m_toOrderBtn = nullptr;
    QPushButton     *m_foreignBtn = nullptr;   // parts of linked songs into the order
    bool             m_updatingLinks = false;
    RichTextEdit    *m_text = nullptr;
    QListWidget     *m_order = nullptr;
    QCheckBox       *m_asDefault = nullptr;
    BackgroundPicker *m_background = nullptr;
    QPushButton     *m_okBtn = nullptr;
};
