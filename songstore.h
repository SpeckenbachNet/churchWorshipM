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

#include <QHash>
#include <QList>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

#include "song.h"

// The song library.
//   <AppData>/songs/songs.sqlite
//     songs (id, title, ccli, data, modified)     data = song as JSON (parts, order, ...)
class SongStore : public QObject {
    Q_OBJECT

public:
    explicit SongStore(QObject *parent = nullptr);
    ~SongStore() override;

    static QString dir();

    QList<Song> songs() const;          // sorted by title
    Song        song(const QString &id) const;
    Song        findByCcli(const QString &ccliNumber) const;

    QString add(Song song);             // returns the new id
    bool    update(const Song &song);
    bool    remove(const QString &id);

    // Translations: songs linked with each other (same group), sorted by language name
    QList<Song> translations(const QString &id) const;
    // Links the song with exactly these songs (empty: removes the song from its links)
    bool        setTranslations(const QString &id, const QStringList &linkedIds);

    // SongSelect lyrics file (recognized by its CCLI song number). A song with the same CCLI
    // number is replaced (its own order is kept if the parts still exist), otherwise added.
    // 'licence' receives the church licence number from the file footer.
    QString importSongSelect(const QString &path, QString *licence, QString *error);

signals:
    void changed();

private:
    void load();
    bool save(const Song &song);

    QSqlDatabase         m_db;
    QHash<QString, Song> m_songs;
};
