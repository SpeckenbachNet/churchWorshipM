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

#include <QColor>
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
    QString text;    // lines of the part (TextMarkup); a line "---" forces a new slide

    QString label() const;   // translated display name, e.g. "Vers 1"
    static QString idPrefix(Kind kind);   // "V", "C", "B", ...
    static QColor  color(Kind kind);      // marker color: verse blue, chorus orange, ...
    QJsonObject toJson() const;
    static SongPart fromJson(const QJsonObject &o);
};

struct Song;

// One slide of an arranged song
struct SongSlide {
    SongPart part;       // part the slide belongs to
    QString  songId;     // song of the part: a verse can be sung in another language
    QString  language;
    QString  text;       // lyrics (TextMarkup)
    QString  below;      // translation shown smaller below, empty for none
    bool     foreign = false;   // part of a linked song, not of the arranged song itself
};

// A song of the song library
struct Song {
    QString         id;          // UUID
    QString         title;
    QString         authors;
    QString         copyright;
    QString         ccliNumber;  // CCLI song number, empty for own songs
    QList<SongPart> parts;       // every part once
    QStringList     order;       // part ids in the order they are sung, may repeat;
                                 // "V3@<song id>": part of a linked song (other language)
    QString         language = defaultLanguage();   // ISO code: "de", "en", ...
    QString         group;       // songs with the same group are translations of each other
    QDateTime       modified;

    bool isValid() const { return !id.isEmpty(); }
    int  partIndex(const QString &partId) const;

    // Parts in the order they are sung; unknown ids are skipped.
    // An empty order means every part once in the stored sequence.
    QList<SongPart> arrangedParts(const QStringList &order = {}) const;

    // The slides: one per part, a line "---" or an empty line splits a part.
    // 'linked': songs linked with this one, for parts of the order in another language.
    // 'translation' (if set) is shown below: matched by part id (V1 - V1), within a part
    // slide by slide. Below a part of a linked song the song itself is shown.
    QList<SongSlide> slides(const QStringList &order = {}, const QList<Song> &linked = {},
                            const Song *translation = nullptr) const;
    // Slide text of an entry: slides separated by an empty line
    static QString slideText(const QList<SongSlide> &slides);

    QString allText() const;   // every part once, for the full text search

    // Parts of the order that the translation does not have (each once, own parts only)
    QList<SongPart> missingIn(const Song &translation, const QStringList &order = {}) const;

    QJsonObject toJson() const;
    static Song fromJson(const QJsonObject &o);

    // Order ids of parts of linked songs: "V3@<song id>"
    static QString foreignPartId(const QString &partId, const QString &songId);
    static QString splitPartId(const QString &id, QString *songId);   // songId empty: own part

    static QString     defaultLanguage() { return QStringLiteral("de"); }
    static QStringList languages();                        // codes offered for selection
    static QString     languageName(const QString &code);  // translated, e.g. "Englisch"

    // SongSelect "Lyrics" download (*.txt). The church licence number printed in the
    // footer is returned in 'licence'. Returns an invalid song (empty title) on failure.
    static Song fromSongSelectText(const QString &text, QString *licence = nullptr);
};
