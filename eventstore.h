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
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QTime>

#include "slidebackground.h"

// One event (service, concert, ...) or a template for new events
struct EventInfo {
    QString   id;                  // UUID
    QString   name;
    QDate     date;                // empty for templates
    QTime     time;
    QString   note;
    SlideBackground background{SlideBackground::Black};   // default of the text slides
    bool      isTemplate = false;
    int       itemCount = 0;       // number of playlist entries (read only)
    QDateTime modified;

    bool isValid() const { return !id.isEmpty(); }
    bool isPast() const { return !isTemplate && date < QDate::currentDate(); }
};

// All events and templates with their playlists.
//   <AppData>/events/events.sqlite
//     events (id, name, date, time, note, template, modified, background)   background = JSON
//     items  (event, pos, item)       item = playlist entry as JSON (same format as in *.cwm)
// A template is an event without date; new events copy its playlist.
class EventStore : public QObject {
    Q_OBJECT

public:
    explicit EventStore(QObject *parent = nullptr);
    ~EventStore() override;

    static QString dir();

    QList<EventInfo> events() const;      // without templates, newest first
    QList<EventInfo> templates() const;   // sorted by name
    EventInfo        event(const QString &id) const;
    EventInfo        nextUpcoming() const;   // earliest event from today on

    // Returns the id of the new event; 'items' is the initial playlist
    QString create(EventInfo info, const QJsonArray &items = {});
    bool    update(const EventInfo &info);   // name, date, time, note, background
    bool    remove(const QString &id);

    QJsonArray items(const QString &id) const;
    bool       setItems(const QString &id, const QJsonArray &items);

    // Exchange with other computers (*.cwm). Import creates a new event.
    bool    exportFile(const QString &id, const QString &path, QString *error) const;
    QString importFile(const QString &path, QString *error);

signals:
    void changed();

private:
    void load();
    bool save(const EventInfo &e);
    bool writeItems(const QString &id, const QJsonArray &items);

    QSqlDatabase               m_db;
    QHash<QString, EventInfo>  m_events;
};
