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

#include "mediaitem.h"
#include "slidebackground.h"

#include <QDialog>
#include <QTimer>

class QLabel;
class QLineEdit;
class QListWidget;
class RichTextEdit;
class QPushButton;

// Creates / edits a text entry: own slides, and song or bible entries that are not
// (any more) connected to the song library or an installed bible.
// The type is fixed by the caller; the slides are previewed while typing.
class TextSlideDialog : public QDialog {
    Q_OBJECT

public:
    explicit TextSlideDialog(MediaItem::Type type, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    void setText(const QString &text);
    // Background of the preview (the one the entry is shown with)
    void setPreviewBackground(const SlideBackground &background);

    MediaItem::Type type() const { return m_type; }
    QString title() const;   // first line of the text if no title was entered
    QString text() const;

private:
    void updatePreview();
    void updateOk();

    MediaItem::Type m_type;
    QLineEdit      *m_titleEdit = nullptr;
    RichTextEdit   *m_textEdit  = nullptr;
    SlideBackground m_background{SlideBackground::Black};
    QListWidget    *m_preview   = nullptr;
    QLabel         *m_count     = nullptr;
    QPushButton    *m_okBtn     = nullptr;
    QTimer          m_previewTimer;
};
