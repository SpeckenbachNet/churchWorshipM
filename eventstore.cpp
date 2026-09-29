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
#include "eventstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {

const QString kConnection = QStringLiteral("events");

// Version 1: only "items" (playlist files before the event management)
// Version 2: additionally "event" with name, date, time, note, template
constexpr int kFileVersion = 2;

} // namespace

QString EventStore::dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/events";
}

EventStore::EventStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(dir());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnection);
    m_db.setDatabaseName(dir() + "/events.sqlite");
    if (m_db.open()) {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS events ("
                              "id TEXT PRIMARY KEY, name TEXT, date TEXT, time TEXT, note TEXT, "
                              "template INTEGER, modified TEXT)"));
        q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS items ("
                              "event TEXT, pos INTEGER, item TEXT, PRIMARY KEY (event, pos))"));
        load();
    }
}

EventStore::~EventStore()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnection);
}

void EventStore::load()
{
    QSqlQuery q(QStringLiteral("SELECT e.id, e.name, e.date, e.time, e.note, e.template, e.modified, "
                               "(SELECT COUNT(*) FROM items i WHERE i.event = e.id) FROM events e"), m_db);
    while (q.next()) {
        EventInfo e;
        e.id = q.value(0).toString();
        e.name = q.value(1).toString();
        e.date = QDate::fromString(q.value(2).toString(), Qt::ISODate);
        e.time = QTime::fromString(q.value(3).toString(), Qt::ISODate);
        e.note = q.value(4).toString();
        e.isTemplate = q.value(5).toBool();
        e.modified = QDateTime::fromString(q.value(6).toString(), Qt::ISODate);
        e.itemCount = q.value(7).toInt();
        m_events.insert(e.id, e);
    }
}

bool EventStore::save(const EventInfo &e)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO events (id, name, date, time, note, template, modified) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(e.id);
    q.addBindValue(e.name);
    q.addBindValue(e.date.toString(Qt::ISODate));
    q.addBindValue(e.time.toString(Qt::ISODate));
    q.addBindValue(e.note);
    q.addBindValue(e.isTemplate);
    q.addBindValue(e.modified.toString(Qt::ISODate));
    if (!q.exec()) {
        return false;
    }
    m_events.insert(e.id, e);
    return true;
}

bool EventStore::writeItems(const QString &id, const QJsonArray &items)
{
    // Whole playlist in one transaction: a crash never leaves half a playlist behind
    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM items WHERE event = ?"));
    q.addBindValue(id);
    bool ok = q.exec();

    q.prepare(QStringLiteral("INSERT INTO items (event, pos, item) VALUES (?, ?, ?)"));
    for (int i = 0; ok && i < items.size(); ++i) {
        q.addBindValue(id);
        q.addBindValue(i);
        q.addBindValue(QString::fromUtf8(QJsonDocument(items.at(i).toObject()).toJson(QJsonDocument::Compact)));
        ok = q.exec();
    }
    if (!ok) {
        m_db.rollback();
        return false;
    }
    return m_db.commit();
}

QList<EventInfo> EventStore::events() const
{
    QList<EventInfo> list;
    for (const EventInfo &e : m_events) {
        if (!e.isTemplate) {
            list << e;
        }
    }
    std::sort(list.begin(), list.end(), [](const EventInfo &a, const EventInfo &b) {
        return a.date != b.date ? a.date > b.date : a.time > b.time;
    });
    return list;
}

QList<EventInfo> EventStore::templates() const
{
    QList<EventInfo> list;
    for (const EventInfo &e : m_events) {
        if (e.isTemplate) {
            list << e;
        }
    }
    std::sort(list.begin(), list.end(), [](const EventInfo &a, const EventInfo &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return list;
}

EventInfo EventStore::event(const QString &id) const
{
    return m_events.value(id);
}

EventInfo EventStore::nextUpcoming() const
{
    EventInfo next;
    for (const EventInfo &e : m_events) {
        if (e.isTemplate || e.isPast()) {
            continue;
        }
        if (!next.isValid() || e.date < next.date || (e.date == next.date && e.time < next.time)) {
            next = e;
        }
    }
    return next;
}

QString EventStore::create(EventInfo info, const QJsonArray &items)
{
    info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.modified = QDateTime::currentDateTime();
    info.itemCount = int(items.size());
    if (info.isTemplate) {
        info.date = QDate();
        info.time = QTime();
    }
    if (!save(info) || !writeItems(info.id, items)) {
        return {};
    }
    emit changed();
    return info.id;
}

bool EventStore::update(const EventInfo &info)
{
    EventInfo e = event(info.id);
    if (!e.isValid()) {
        return false;
    }
    e.name = info.name;
    e.note = info.note;
    if (!e.isTemplate) {
        e.date = info.date;
        e.time = info.time;
    }
    e.modified = QDateTime::currentDateTime();
    if (!save(e)) {
        return false;
    }
    emit changed();
    return true;
}

bool EventStore::remove(const QString &id)
{
    if (!m_events.contains(id)) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM items WHERE event = ?"));
    q.addBindValue(id);
    q.exec();
    q.prepare(QStringLiteral("DELETE FROM events WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    m_events.remove(id);
    emit changed();
    return true;
}

QJsonArray EventStore::items(const QString &id) const
{
    QJsonArray items;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT item FROM items WHERE event = ? ORDER BY pos"));
    q.addBindValue(id);
    q.exec();
    while (q.next()) {
        items.append(QJsonDocument::fromJson(q.value(0).toString().toUtf8()).object());
    }
    return items;
}

bool EventStore::setItems(const QString &id, const QJsonArray &items)
{
    EventInfo e = event(id);
    if (!e.isValid() || !writeItems(id, items)) {
        return false;
    }
    e.itemCount = int(items.size());
    e.modified = QDateTime::currentDateTime();
    if (!save(e)) {
        return false;
    }
    emit changed();
    return true;
}

// ---------------------------------------------------------------------------
// Import / export
// ---------------------------------------------------------------------------

bool EventStore::exportFile(const QString &id, const QString &path, QString *error) const
{
    const EventInfo e = event(id);
    if (!e.isValid()) {
        return false;
    }
    QJsonObject info{{"name", e.name}, {"note", e.note}};
    if (e.isTemplate) {
        info.insert("template", true);
    } else {
        info.insert("date", e.date.toString(Qt::ISODate));
        info.insert("time", e.time.toString(Qt::ISODate));
    }
    const QJsonObject root{{"version", kFileVersion}, {"event", info}, {"items", items(id)}};

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        *error = tr("Cannot write %1.").arg(path);
        return false;
    }
    file.write(QJsonDocument(root).toJson());
    if (!file.commit()) {
        *error = tr("Cannot write %1.").arg(path);
        return false;
    }
    return true;
}

QString EventStore::importFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = tr("Cannot open %1.").arg(path);
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.object().contains("items")) {
        *error = tr("%1 is not an event file.").arg(QFileInfo(path).fileName());
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonObject info = root.value("event").toObject();

    // Version 1 files have no event data: name from the file, date = today
    EventInfo e;
    e.name = info.value("name").toString(QFileInfo(path).completeBaseName());
    e.note = info.value("note").toString();
    e.isTemplate = info.value("template").toBool();
    e.date = QDate::fromString(info.value("date").toString(), Qt::ISODate);
    e.time = QTime::fromString(info.value("time").toString(), Qt::ISODate);
    if (!e.isTemplate && !e.date.isValid()) {
        e.date = QDate::currentDate();
    }

    const QString id = create(e, root.value("items").toArray());
    if (id.isEmpty()) {
        *error = m_db.lastError().text();
    }
    return id;
}
