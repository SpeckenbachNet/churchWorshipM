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

#include <QLineEdit>

// Search field in the look of the ToolbarM buttons: rounded "pill", magnifier on the left,
// clear button on the right. Colors are derived from the palette (light and dark mode).
class SearchField : public QLineEdit {
    Q_OBJECT

public:
    explicit SearchField(QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateLook();

    QAction *m_searchAction = nullptr;
    bool     m_updating = false;   // setStyleSheet() itself triggers style/palette events
};
