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

#include "eventstore.h"

#include <QDialog>
#include <QJsonArray>
#include <QPrinter>

class QCheckBox;
class QLabel;
class QPrintPreviewWidget;
class SongStore;

// The plan of an event for the band, technicians and moderation:
//   Lied 1:     Zehntausend Gründe
//   Bibeltext:  Johannes 3,16–18 (NeÜ)
// Optionally with the order of the song parts, the lyrics and the bible texts.
struct PrintOptions {
    bool songOrder = false;
    bool lyrics = false;
    bool bibleText = false;

    static PrintOptions load();   // last choice
    void save() const;
};

// Options on the left, the pages as they will be printed on the right (updated right away);
// "Print..." opens the system print dialog, "Save as PDF..." writes a PDF file directly.
class PrintPlanDialog : public QDialog {
    Q_OBJECT

public:
    PrintPlanDialog(const EventInfo &event, const QJsonArray &items, const SongStore *songs,
                    QWidget *parent = nullptr);
    PrintOptions options() const;

private:
    void printTo(QPrinter *printer);
    void print();
    void savePdf();

    EventInfo        m_event;
    QJsonArray       m_items;
    const SongStore *m_songs;
    QPrinter         m_printer{QPrinter::HighResolution};

    QCheckBox           *m_order = nullptr;
    QCheckBox           *m_lyrics = nullptr;
    QCheckBox           *m_bible = nullptr;
    QPrintPreviewWidget *m_preview = nullptr;
    QLabel              *m_pages = nullptr;
};

namespace PrintPlan {

// 'items': playlist entries as saved in the event
QString html(const EventInfo &event, const QJsonArray &items, const PrintOptions &options, const SongStore *songs);

// The plan window with preview, printing and PDF export
void print(QWidget *parent, const EventInfo &event, const QJsonArray &items, const SongStore *songs);

} // namespace PrintPlan
