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

#include <QDialog>

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

// Create / edit a text based entry (song, bible text, own slide)
class TextSlideDialog : public QDialog {
    Q_OBJECT

public:
    explicit TextSlideDialog(QWidget *parent = nullptr);

    void setType(MediaItem::Type type);
    void setTitle(const QString &title);
    void setText(const QString &text);

    MediaItem::Type type() const;
    QString title() const;
    QString text() const;

private:
    QComboBox      *m_typeCombo = nullptr;
    QLineEdit      *m_titleEdit = nullptr;
    QPlainTextEdit *m_textEdit  = nullptr;
};
