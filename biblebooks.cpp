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
#include "biblebooks.h"

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>

namespace {

struct BookDef {
    int         nr;
    const char *osis;
    const char *german;
};

// Short names for tiles, same order as kBooks
const char *const kGermanAbbreviations[] = {
    "1Mo", "2Mo", "3Mo", "4Mo", "5Mo", "Jos", "Ri", "Rut", "1Sam", "2Sam", "1Kön", "2Kön",
    "1Chr", "2Chr", "Esr", "Neh", "Est", "Hiob", "Ps", "Spr", "Pred", "Hld", "Jes", "Jer",
    "Klgl", "Hes", "Dan", "Hos", "Joel", "Am", "Obd", "Jona", "Mi", "Nah", "Hab", "Zef",
    "Hag", "Sach", "Mal",
    "Mt", "Mk", "Lk", "Joh", "Apg", "Röm", "1Kor", "2Kor", "Gal", "Eph", "Phil", "Kol",
    "1Thess", "2Thess", "1Tim", "2Tim", "Tit", "Phlm", "Hebr", "Jak", "1Petr", "2Petr",
    "1Joh", "2Joh", "3Joh", "Jud", "Offb",
    "Jdt", "Weish", "Tob", "Sir", "Bar", "1Makk", "2Makk", "StEst", "StDan", "GebMan",
};

// Names follow the Loccumer Richtlinien
const BookDef kBooks[] = {
    { 1, "Gen",    "1. Mose"},          { 2, "Exod",   "2. Mose"},
    { 3, "Lev",    "3. Mose"},          { 4, "Num",    "4. Mose"},
    { 5, "Deut",   "5. Mose"},          { 6, "Josh",   "Josua"},
    { 7, "Judg",   "Richter"},          { 8, "Ruth",   "Rut"},
    { 9, "1Sam",   "1. Samuel"},        {10, "2Sam",   "2. Samuel"},
    {11, "1Kgs",   "1. Könige"},        {12, "2Kgs",   "2. Könige"},
    {13, "1Chr",   "1. Chronik"},       {14, "2Chr",   "2. Chronik"},
    {15, "Ezra",   "Esra"},             {16, "Neh",    "Nehemia"},
    {17, "Esth",   "Ester"},            {18, "Job",    "Hiob"},
    {19, "Ps",     "Psalm"},            {20, "Prov",   "Sprüche"},
    {21, "Eccl",   "Prediger"},         {22, "Song",   "Hoheslied"},
    {23, "Isa",    "Jesaja"},           {24, "Jer",    "Jeremia"},
    {25, "Lam",    "Klagelieder"},      {26, "Ezek",   "Hesekiel"},
    {27, "Dan",    "Daniel"},           {28, "Hos",    "Hosea"},
    {29, "Joel",   "Joel"},             {30, "Amos",   "Amos"},
    {31, "Obad",   "Obadja"},           {32, "Jonah",  "Jona"},
    {33, "Mic",    "Micha"},            {34, "Nah",    "Nahum"},
    {35, "Hab",    "Habakuk"},          {36, "Zeph",   "Zefanja"},
    {37, "Hag",    "Haggai"},           {38, "Zech",   "Sacharja"},
    {39, "Mal",    "Maleachi"},
    {40, "Matt",   "Matthäus"},         {41, "Mark",   "Markus"},
    {42, "Luke",   "Lukas"},            {43, "John",   "Johannes"},
    {44, "Acts",   "Apostelgeschichte"},{45, "Rom",    "Römer"},
    {46, "1Cor",   "1. Korinther"},     {47, "2Cor",   "2. Korinther"},
    {48, "Gal",    "Galater"},          {49, "Eph",    "Epheser"},
    {50, "Phil",   "Philipper"},        {51, "Col",    "Kolosser"},
    {52, "1Thess", "1. Thessalonicher"},{53, "2Thess", "2. Thessalonicher"},
    {54, "1Tim",   "1. Timotheus"},     {55, "2Tim",   "2. Timotheus"},
    {56, "Titus",  "Titus"},            {57, "Phlm",   "Philemon"},
    {58, "Heb",    "Hebräer"},          {59, "Jas",    "Jakobus"},
    {60, "1Pet",   "1. Petrus"},        {61, "2Pet",   "2. Petrus"},
    {62, "1John",  "1. Johannes"},      {63, "2John",  "2. Johannes"},
    {64, "3John",  "3. Johannes"},      {65, "Jude",   "Judas"},
    {66, "Rev",    "Offenbarung"},
    // Apocrypha
    {67, "Jdt",     "Judit"},           {68, "Wis",    "Weisheit"},
    {69, "Tob",     "Tobit"},           {70, "Sir",    "Jesus Sirach"},
    {71, "Bar",     "Baruch"},          {72, "1Macc",  "1. Makkabäer"},
    {73, "2Macc",   "2. Makkabäer"},    {74, "AddEsth","Stücke zu Ester"},
    {75, "AddDan",  "Stücke zu Daniel"},{76, "PrMan",  "Gebet des Manasse"},
};

const BookDef *findByNumber(int nr)
{
    for (const BookDef &b : kBooks) {
        if (b.nr == nr) {
            return &b;
        }
    }
    return nullptr;
}

QList<Versification::Book> parseBooks(const QJsonArray &array)
{
    QList<Versification::Book> books;
    for (const QJsonValue &v : array) {
        const QJsonArray entry = v.toArray();
        Versification::Book book;
        book.osisId = entry.at(0).toString();
        for (const QJsonValue &count : entry.at(1).toArray()) {
            book.verseCounts << count.toInt();
        }
        books << book;
    }
    return books;
}

} // namespace

namespace BibleBooks {

int numberFromOsis(const QString &osisId)
{
    for (const BookDef &b : kBooks) {
        if (osisId == QLatin1String(b.osis)) {
            return b.nr;
        }
    }
    return 0;
}

QString osisId(int nr)
{
    const BookDef *b = findByNumber(nr);
    return b ? QString::fromLatin1(b->osis) : QString();
}

QString germanName(int nr)
{
    const BookDef *b = findByNumber(nr);
    return b ? QString::fromUtf8(b->german) : QString();
}

QString germanAbbreviation(int nr)
{
    constexpr int count = int(sizeof(kGermanAbbreviations) / sizeof(kGermanAbbreviations[0]));
    return (nr >= 1 && nr <= count) ? QString::fromUtf8(kGermanAbbreviations[nr - 1]) : QString();
}

QList<int> allNumbers()
{
    QList<int> numbers;
    for (const BookDef &b : kBooks) {
        numbers << b.nr;
    }
    return numbers;
}

} // namespace BibleBooks

const Versification *Versification::get(const QString &name)
{
    // Loaded once from the resource, importers may run in a worker thread
    static QMutex mutex;
    static QHash<QString, Versification> systems;
    const QMutexLocker lock(&mutex);

    if (systems.isEmpty()) {
        QFile file(QStringLiteral(":/bible/versifications.json"));
        if (!file.open(QIODevice::ReadOnly)) {
            return nullptr;
        }
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = root.begin(); it != root.end(); ++it) {
            const QJsonObject o = it.value().toObject();
            systems.insert(it.key(), {parseBooks(o.value("ot").toArray()),
                                      parseBooks(o.value("nt").toArray())});
        }
    }

    const auto it = systems.constFind(name);
    return it == systems.constEnd() ? nullptr : &it.value();
}
