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
#include <QSqlDatabase>
#include <QString>

// Our own bible format: one SQLite file per translation in <AppData>/bibles/<id>.sqlite
//
//   meta   (key, value)                          name, abbreviation, language, license, ...
//   books  (nr, osis, name, chapters)            nr = BibleBooks numbering
//   verses (book, chapter, verse, text, heading) heading = section title before the verse
//
// Every import format (SWORD, getBible, OSIS, Zefania) is converted into this once.

struct BibleInfo {
    QString id;             // file name without suffix, e.g. "gerneue"
    QString name;           // "Neue evangelistische Übersetzung"
    QString abbreviation;   // "NeÜ"
    QString language;       // "de"
    QString license;        // shown to the user
    QString copyright;
    QString source;         // where it came from (URL / file)
    QString path;           // SQLite file
};

struct BibleVerse {
    int     book = 0;
    int     chapter = 0;
    int     verse = 0;
    QString text;
    QString heading;
};

// Collapses spaces, trims every line and drops empty lines (used by all importers)
namespace BibleText {
QString normalize(const QString &text);
}

namespace BibleStore {

QString biblesDir();
QString pathFor(const QString &id);
QList<BibleInfo> installed();
bool remove(const QString &id);

} // namespace BibleStore

// Read access to one installed bible
class Bible {
public:
    Bible() = default;
    ~Bible();
    Bible(const Bible &) = delete;
    Bible &operator=(const Bible &) = delete;

    bool open(const QString &id, QString *error = nullptr);
    bool isOpen() const { return m_db.isOpen(); }
    const BibleInfo &info() const { return m_info; }

    QList<int> books() const;
    QString    bookName(int book) const;
    int        chapterCount(int book) const;
    int        verseCount(int book, int chapter) const;

    // Verses 'from'..'to' of one chapter (to = -1: until the end of the chapter)
    QList<BibleVerse> verses(int book, int chapter, int from = 1, int to = -1) const;

private:
    QSqlDatabase m_db;
    QString      m_connection;
    BibleInfo    m_info;
};

// Writes a new bible. Used by all importers.
// Writes into a temporary file; only commit() replaces an existing bible with the same id.
class BibleWriter {
public:
    BibleWriter() = default;
    ~BibleWriter();
    BibleWriter(const BibleWriter &) = delete;
    BibleWriter &operator=(const BibleWriter &) = delete;

    bool begin(const BibleInfo &info, QString *error);
    void addBook(int nr, const QString &name, int chapters);
    void addVerse(int book, int chapter, int verse, const QString &text, const QString &heading = {});
    bool commit(QString *error);
    int  verseCount() const { return m_verses; }

private:
    void close();

    QSqlDatabase m_db;
    QString      m_connection;
    QString      m_tmpPath;
    QString      m_targetPath;
    int          m_verses = 0;
};
