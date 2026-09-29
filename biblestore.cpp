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
#include "biblestore.h"
#include "biblebooks.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

constexpr int kFormatVersion = 1;

// QSqlDatabase needs a unique connection name per open database (and per thread)
QString newConnectionName()
{
    static QAtomicInt counter;
    return QStringLiteral("bible_%1").arg(counter.fetchAndAddRelaxed(1));
}

QSqlDatabase openSqlite(const QString &connection, const QString &path, bool readOnly)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
    db.setDatabaseName(path);
    if (readOnly) {
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
    }
    db.open();
    return db;
}

void closeSqlite(QSqlDatabase &db, const QString &connection)
{
    if (connection.isEmpty()) {
        return;
    }
    db.close();
    db = QSqlDatabase();   // release the handle, otherwise removeDatabase() warns
    QSqlDatabase::removeDatabase(connection);
}

BibleInfo readInfo(QSqlDatabase &db, const QString &id, const QString &path)
{
    BibleInfo info;
    info.id = id;
    info.path = path;
    QSqlQuery q(QStringLiteral("SELECT key, value FROM meta"), db);
    while (q.next()) {
        const QString key = q.value(0).toString();
        const QString value = q.value(1).toString();
        if      (key == "name")         info.name = value;
        else if (key == "abbreviation") info.abbreviation = value;
        else if (key == "language")     info.language = value;
        else if (key == "license")      info.license = value;
        else if (key == "copyright")    info.copyright = value;
        else if (key == "source")       info.source = value;
    }
    return info;
}

} // namespace

// ---------------------------------------------------------------------------
// BibleText
// ---------------------------------------------------------------------------

namespace BibleText {

QString normalize(const QString &text)
{
    static const QRegularExpression spaces(QStringLiteral("[ \\t\\r\\x{00a0}]+"));
    QStringList lines;
    for (QString line : text.split('\n')) {
        line = line.replace(spaces, QStringLiteral(" ")).trimmed();
        if (!line.isEmpty()) {
            lines << line;
        }
    }
    return lines.join('\n');
}

} // namespace BibleText

// ---------------------------------------------------------------------------
// BibleStore
// ---------------------------------------------------------------------------

namespace BibleStore {

QString biblesDir()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/bibles";
    QDir().mkpath(dir);
    return dir;
}

QString pathFor(const QString &id)
{
    return biblesDir() + "/" + id + ".sqlite";
}

QList<BibleInfo> installed()
{
    QList<BibleInfo> list;
    const QFileInfoList files = QDir(biblesDir()).entryInfoList({"*.sqlite"}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : files) {
        Bible bible;
        if (bible.open(fi.completeBaseName())) {
            list << bible.info();
        }
    }
    return list;
}

bool remove(const QString &id)
{
    return QFile::remove(pathFor(id));
}

} // namespace BibleStore

// ---------------------------------------------------------------------------
// Bible (read)
// ---------------------------------------------------------------------------

Bible::~Bible()
{
    closeSqlite(m_db, m_connection);
}

bool Bible::open(const QString &id, QString *error)
{
    closeSqlite(m_db, m_connection);

    const QString path = BibleStore::pathFor(id);
    if (!QFileInfo::exists(path)) {
        if (error) *error = QCoreApplication::translate("BibleStore", "Bible not installed.");
        return false;
    }

    m_connection = newConnectionName();
    m_db = openSqlite(m_connection, path, true);
    if (!m_db.isOpen()) {
        if (error) *error = m_db.lastError().text();
        return false;
    }
    m_info = readInfo(m_db, id, path);
    return true;
}

QList<int> Bible::books() const
{
    QList<int> list;
    QSqlQuery q(QStringLiteral("SELECT nr FROM books ORDER BY nr"), m_db);
    while (q.next()) {
        list << q.value(0).toInt();
    }
    return list;
}

QString Bible::bookName(int book) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT name FROM books WHERE nr = ?"));
    q.addBindValue(book);
    return q.exec() && q.next() ? q.value(0).toString() : QString();
}

int Bible::chapterCount(int book) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT MAX(chapter) FROM verses WHERE book = ?"));
    q.addBindValue(book);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

int Bible::verseCount(int book, int chapter) const
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT MAX(verse) FROM verses WHERE book = ? AND chapter = ?"));
    q.addBindValue(book);
    q.addBindValue(chapter);
    return q.exec() && q.next() ? q.value(0).toInt() : 0;
}

QList<BibleVerse> Bible::verses(int book, int chapter, int from, int to) const
{
    QList<BibleVerse> list;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT verse, text, heading FROM verses "
                             "WHERE book = ? AND chapter = ? AND verse >= ? AND verse <= ? "
                             "ORDER BY verse"));
    q.addBindValue(book);
    q.addBindValue(chapter);
    q.addBindValue(from);
    q.addBindValue(to < 0 ? 1000 : to);
    if (!q.exec()) {
        return list;
    }
    while (q.next()) {
        list << BibleVerse{book, chapter, q.value(0).toInt(), q.value(1).toString(), q.value(2).toString()};
    }
    return list;
}

// ---------------------------------------------------------------------------
// BibleWriter
// ---------------------------------------------------------------------------

BibleWriter::~BibleWriter()
{
    // Not committed -> throw the half written file away
    close();
    if (!m_tmpPath.isEmpty()) {
        QFile::remove(m_tmpPath);
    }
}

void BibleWriter::close()
{
    closeSqlite(m_db, m_connection);
    m_connection.clear();
}

bool BibleWriter::begin(const BibleInfo &info, QString *error)
{
    m_targetPath = BibleStore::pathFor(info.id);
    m_tmpPath = m_targetPath + ".part";
    QFile::remove(m_tmpPath);

    m_connection = newConnectionName();
    m_db = openSqlite(m_connection, m_tmpPath, false);
    if (!m_db.isOpen()) {
        *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery q(m_db);
    const QStringList schema = {
        "PRAGMA journal_mode = OFF",
        "PRAGMA synchronous = OFF",
        "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT)",
        "CREATE TABLE books (nr INTEGER PRIMARY KEY, osis TEXT, name TEXT, chapters INTEGER)",
        "CREATE TABLE verses (book INTEGER, chapter INTEGER, verse INTEGER, text TEXT, heading TEXT, "
        "PRIMARY KEY (book, chapter, verse)) WITHOUT ROWID",
    };
    for (const QString &sql : schema) {
        if (!q.exec(sql)) {
            *error = q.lastError().text();
            return false;
        }
    }

    const QList<QPair<QString, QString>> meta = {
        {"format_version", QString::number(kFormatVersion)},
        {"name", info.name}, {"abbreviation", info.abbreviation}, {"language", info.language},
        {"license", info.license}, {"copyright", info.copyright}, {"source", info.source},
    };
    q.prepare(QStringLiteral("INSERT INTO meta (key, value) VALUES (?, ?)"));
    for (const auto &kv : meta) {
        q.addBindValue(kv.first);
        q.addBindValue(kv.second);
        q.exec();
    }

    m_db.transaction();   // one big transaction: thousands of times faster
    return true;
}

void BibleWriter::addBook(int nr, const QString &name, int chapters)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO books (nr, osis, name, chapters) VALUES (?, ?, ?, ?)"));
    q.addBindValue(nr);
    q.addBindValue(BibleBooks::osisId(nr));
    q.addBindValue(name);
    q.addBindValue(chapters);
    q.exec();
}

void BibleWriter::addVerse(int book, int chapter, int verse, const QString &text, const QString &heading)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO verses (book, chapter, verse, text, heading) "
                             "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(book);
    q.addBindValue(chapter);
    q.addBindValue(verse);
    q.addBindValue(text);
    q.addBindValue(heading.isEmpty() ? QVariant() : QVariant(heading));
    if (q.exec()) {
        ++m_verses;
    }
}

bool BibleWriter::commit(QString *error)
{
    if (m_verses == 0) {
        *error = QCoreApplication::translate("BibleStore", "The file contains no bible verses.");
        return false;
    }
    if (!m_db.commit()) {
        *error = m_db.lastError().text();
        return false;
    }
    close();

    QFile::remove(m_targetPath);
    if (!QFile::rename(m_tmpPath, m_targetPath)) {
        *error = QCoreApplication::translate("BibleStore", "The bible could not be saved.");
        return false;
    }
    m_tmpPath.clear();
    return true;
}
