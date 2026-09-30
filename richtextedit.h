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

class QTextEdit;
class QToolButton;

// Text field for slide texts with bold / italic / underline (buttons and Ctrl/Cmd+B, I, U).
// Works with the markup of TextMarkup; pasted text keeps only these three formats.
class RichTextEdit : public QWidget {
    Q_OBJECT

public:
    explicit RichTextEdit(QWidget *parent = nullptr);

    void    setMarkup(const QString &markup);
    QString markup() const;
    QString plainText() const;

    void setPlaceholderText(const QString &text);

signals:
    void textChanged();

private:
    void updateButtons();

    QTextEdit   *m_edit = nullptr;
    QToolButton *m_boldBtn = nullptr;
    QToolButton *m_italicBtn = nullptr;
    QToolButton *m_underlineBtn = nullptr;
};
