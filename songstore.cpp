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
#include "songstore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {

const QString kConnection = QStringLiteral("songs");

} // namespace

QString SongStore::dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/songs";
}

SongStore::SongStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(dir());

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnection);
    m_db.setDatabaseName(dir() + "/songs.sqlite");
    if (m_db.open()) {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS songs ("
                              "id TEXT PRIMARY KEY, title TEXT, ccli TEXT, data TEXT, modified TEXT)"));
        load();
    }
}

SongStore::~SongStore()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnection);
}

void SongStore::load()
{
    QSqlQuery q(QStringLiteral("SELECT id, data, modified FROM songs"), m_db);
    while (q.next()) {
        Song s = Song::fromJson(QJsonDocument::fromJson(q.value(1).toString().toUtf8()).object());
        s.id = q.value(0).toString();
        s.modified = QDateTime::fromString(q.value(2).toString(), Qt::ISODate);
        m_songs.insert(s.id, s);
    }
}

bool SongStore::save(const Song &song)
{
    // Title and CCLI number as own columns for searching outside the app
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO songs (id, title, ccli, data, modified) "
                             "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(song.id);
    q.addBindValue(song.title);
    q.addBindValue(song.ccliNumber);
    q.addBindValue(QString::fromUtf8(QJsonDocument(song.toJson()).toJson(QJsonDocument::Compact)));
    q.addBindValue(song.modified.toString(Qt::ISODate));
    if (!q.exec()) {
        return false;
    }
    m_songs.insert(song.id, song);
    return true;
}

QList<Song> SongStore::songs() const
{
    QList<Song> list = m_songs.values();
    std::sort(list.begin(), list.end(), [](const Song &a, const Song &b) {
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return list;
}

Song SongStore::song(const QString &id) const
{
    return m_songs.value(id);
}

Song SongStore::findByCcli(const QString &ccliNumber) const
{
    if (!ccliNumber.isEmpty()) {
        for (const Song &s : m_songs) {
            if (s.ccliNumber == ccliNumber) {
                return s;
            }
        }
    }
    return {};
}

QString SongStore::add(Song song)
{
    song.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    song.modified = QDateTime::currentDateTime();
    if (!save(song)) {
        return {};
    }
    emit changed();
    return song.id;
}

bool SongStore::update(const Song &song)
{
    if (!m_songs.contains(song.id)) {
        return false;
    }
    Song s = song;
    s.modified = QDateTime::currentDateTime();
    if (!save(s)) {
        return false;
    }
    emit changed();
    return true;
}

bool SongStore::remove(const QString &id)
{
    if (!m_songs.contains(id)) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM songs WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    m_songs.remove(id);
    emit changed();
    return true;
}

QList<Song> SongStore::translations(const QString &id) const
{
    const QString group = m_songs.value(id).group;
    QList<Song> list;
    if (group.isEmpty()) {
        return list;
    }
    for (const Song &s : m_songs) {
        if (s.group == group && s.id != id) {
            list << s;
        }
    }
    std::sort(list.begin(), list.end(), [](const Song &a, const Song &b) {
        const int c = Song::languageName(a.language).localeAwareCompare(Song::languageName(b.language));
        return c != 0 ? c < 0 : a.title.localeAwareCompare(b.title) < 0;
    });
    return list;
}

bool SongStore::setTranslations(const QString &id, const QStringList &linkedIds)
{
    if (!m_songs.contains(id)) {
        return false;
    }
    QStringList members;   // the new group
    for (const QString &linked : linkedIds) {
        if (linked != id && m_songs.contains(linked) && !members.contains(linked)) {
            members << linked;
        }
    }
    const QString oldGroup = m_songs.value(id).group;
    QString group;
    if (!members.isEmpty()) {
        members.prepend(id);
        group = oldGroup;
        for (int i = 0; group.isEmpty() && i < members.size(); ++i) {
            group = m_songs.value(members.at(i)).group;
        }
        if (group.isEmpty()) {
            group = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
    }

    bool ok = true;
    bool changedAny = false;
    const auto setGroup = [&](Song s, const QString &g) {
        if (s.group != g) {
            s.group = g;
            ok = save(s) && ok;
            changedAny = true;
        }
    };
    // Songs that left the links; a single remaining song is no longer linked
    QStringList rest;
    for (const Song &s : std::as_const(m_songs)) {
        if (!s.group.isEmpty() && (s.group == oldGroup || s.group == group) && !members.contains(s.id)
            && s.id != id) {
            rest << s.id;
        }
    }
    for (const QString &other : std::as_const(rest)) {
        setGroup(m_songs.value(other), rest.size() > 1 && members.isEmpty() ? oldGroup : QString());
    }
    setGroup(m_songs.value(id), group);
    for (const QString &member : std::as_const(members)) {
        setGroup(m_songs.value(member), group);
    }
    if (changedAny) {
        emit changed();
    }
    return ok;
}

QString SongStore::importSongSelect(const QString &path, QString *licence, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = tr("Cannot open %1.").arg(path);
        return {};
    }
    Song imported = Song::fromSongSelectText(QString::fromUtf8(file.readAll()), licence);
    if (imported.title.isEmpty() || imported.parts.isEmpty()) {
        *error = tr("%1 is not a SongSelect lyrics file.").arg(QFileInfo(path).fileName());
        return {};
    }

    // Downloaded again (e.g. corrected lyrics): replace the text, keep the own order
    const Song existing = findByCcli(imported.ccliNumber);
    if (existing.isValid()) {
        imported.id = existing.id;
        imported.language = existing.language;
        imported.group = existing.group;
        const bool orderFits = std::all_of(existing.order.begin(), existing.order.end(),
                                           [&](const QString &id) {
                                               // parts of linked songs are not affected
                                               return id.contains(QLatin1Char('@')) || imported.partIndex(id) >= 0;
                                           });
        if (orderFits && !existing.order.isEmpty()) {
            imported.order = existing.order;
        }
        if (!update(imported)) {
            *error = m_db.lastError().text();
            return {};
        }
        return imported.id;
    }

    const QString id = add(imported);
    if (id.isEmpty()) {
        *error = m_db.lastError().text();
    }
    return id;
}
