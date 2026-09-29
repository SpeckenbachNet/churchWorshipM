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

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

// A selected bible passage: verses of one chapter of one translation.
// Stored in the playlist including the verse texts, so an event can also be shown on a
// computer where this bible is not installed.
struct BiblePassage {
    QString     bibleId;         // BibleStore id, e.g. "gerneue"
    QString     abbreviation;    // "NeÜ"
    QString     bookName;        // "Johannes" (as named by the bible)
    int         book = 0;        // BibleBooks numbering
    int         chapter = 0;
    QList<int>  verses;          // ascending
    QStringList texts;           // one per verse
    bool        wholeChapter = false;
    bool        oneVersePerSlide = false;

    bool isValid() const { return book > 0 && chapter > 0 && !verses.isEmpty(); }

    // "Johannes 3,16–18", "Johannes 3,16.18", "Psalm 23"
    QString reference() const;

    QJsonObject toJson() const;
    static BiblePassage fromJson(const QJsonObject &json);
};
