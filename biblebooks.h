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

#include <QList>
#include <QString>

// Canonical book numbering used in our own bible format, independent of any source:
//   1..66  = Protestant canon (Gen .. Rev)
//   67..76 = apocrypha (Luther order)
// Every importer maps its own book ids to these numbers.
namespace BibleBooks {

constexpr int kFirstApocrypha = 67;

int     numberFromOsis(const QString &osisId);   // 0 if unknown
QString osisId(int nr);                          // empty if unknown
QString germanName(int nr);                      // "1. Mose", "Johannes", ... (empty if unknown)
QString germanAbbreviation(int nr);              // "1Mo", "Joh", ... (Loccumer Richtlinien)
QList<int> allNumbers();

} // namespace BibleBooks

// Chapter/verse layout of a versification system ("KJV", "German", "Luther", ...).
// Data comes from the SWORD project (see tools/canon2json.py).
struct Versification {
    struct Book {
        QString    osisId;
        QList<int> verseCounts;   // index = chapter - 1
    };
    QList<Book> ot;
    QList<Book> nt;

    // nullptr if the system is unknown
    static const Versification *get(const QString &name);
};
