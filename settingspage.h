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

class BibleSettingsPage;
class QTabWidget;

// Application settings, shown as a page of the main window's stacked widget (not modal).
// Every area is its own tab; new areas are added in the constructor.
class SettingsPage : public QWidget {
    Q_OBJECT

public:
    enum Tab { BiblesTab, LibraryTab, SlidesTab };   // order of the tabs

    explicit SettingsPage(QWidget *parent = nullptr);

    void showTab(Tab tab);

private:
    QTabWidget        *m_tabs = nullptr;
    BibleSettingsPage *m_biblePage = nullptr;
};
