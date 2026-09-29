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

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// One part of a song: verse, chorus, bridge, ...
struct SongPart {
    enum Kind { Verse, PreChorus, Chorus, Bridge, Tag, Intro, Interlude, Ending, Other };

    QString id;      // short key used in the order: "V1", "C", "C2", "P", "B", "T", "E", ...
    Kind    kind = Other;
    int     number = 0;   // 1 for "Verse 1", 0 if the part has no number
    QString text;    // lines of the part; a line "---" forces a new slide

    QString label() const;   // translated display name, e.g. "Vers 1"
    QJsonObject toJson() const;
    static SongPart fromJson(const QJsonObject &o);
};

// A song of the song library
struct Song {
    QString         id;          // UUID
    QString         title;
    QString         authors;
    QString         copyright;
    QString         ccliNumber;  // CCLI song number, empty for own songs
    QList<SongPart> parts;       // every part once
    QStringList     order;       // part ids in the order they are sung, may repeat
    QDateTime       modified;

    bool isValid() const { return !id.isEmpty(); }
    int  partIndex(const QString &partId) const;

    // Parts in the order they are sung; unknown ids are skipped.
    // An empty order means every part once in the stored sequence.
    QList<SongPart> arrangedParts(const QStringList &order = {}) const;

    // Text for the slide display: one slide per part, a line "---" splits a part.
    // Slides are separated by an empty line (format of the text entries).
    QString slideText(const QStringList &order = {}) const;

    QString allText() const;   // every part once, for the full text search

    QJsonObject toJson() const;
    static Song fromJson(const QJsonObject &o);

    // SongSelect "Lyrics" download (*.txt). The church licence number printed in the
    // footer is returned in 'licence'. Returns an invalid song (empty title) on failure.
    static Song fromSongSelectText(const QString &text, QString *licence = nullptr);
};
