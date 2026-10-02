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

#include <QDate>
#include <QHash>
#include <QList>
#include <QWidget>

class EventStore;
class SongStore;
class QCheckBox;
class QComboBox;
class QDateEdit;
class QLabel;
class QListWidget;
class QTreeWidget;

// Which songs were sung how often in a period. Built from the events that have taken place
// (no templates, no future events); a song counts once per event.
//   All songs:   every song, sung or "not sung for a long time"
//   CCLI report: songs with a CCLI number, incl. translations shown below and verses in
//                another language (their own CCLI number), for reporting by hand
class StatisticsPage : public QWidget {
    Q_OBJECT

public:
    enum View { AllSongs, CcliReport };

    explicit StatisticsPage(QWidget *parent = nullptr);

    void setStores(EventStore *events, SongStore *songs);
    void setView(View view);
    void refresh();   // reads the events again

    void copyToClipboard();
    void saveCsv();

private:
    struct Use {
        QDate   date;
        QString eventName;
    };
    struct Entry {
        QString     title;
        QString     language;   // code, empty if unknown
        QString     ccli;
        QList<Use>  uses;       // in the period, newest first
        QDate       lastEver;   // also before the period
    };

    void periodChosen();
    void updateTable();
    void showUses();
    QStringList exportLines(QChar separator) const;

    EventStore *m_events = nullptr;
    SongStore  *m_songs = nullptr;

    QHash<QString, Entry> m_sung;   // key: song id, or "title:<title>" for copies only
    QHash<QString, Entry> m_ccli;   // key: CCLI number

    QComboBox   *m_period = nullptr;
    QDateEdit   *m_from = nullptr;
    QDateEdit   *m_to = nullptr;
    QComboBox   *m_view = nullptr;
    QCheckBox   *m_notSung = nullptr;
    QTreeWidget *m_table = nullptr;
    QListWidget *m_usesList = nullptr;
    QLabel      *m_usesTitle = nullptr;
    QLabel      *m_status = nullptr;
};
