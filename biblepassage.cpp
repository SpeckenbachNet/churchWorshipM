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
#include "biblepassage.h"

#include <QJsonArray>

QString BiblePassage::reference() const
{
    const QString chapterRef = QStringLiteral("%1 %2").arg(bookName).arg(chapter);
    if (wholeChapter || verses.isEmpty()) {
        return chapterRef;
    }

    // Consecutive verses become ranges: 1,2,3,5,7,8 -> "1–3.5.7–8" (German style)
    QStringList parts;
    int start = verses.first();
    int prev = start;
    auto flush = [&] {
        parts << (start == prev ? QString::number(start)
                                : QStringLiteral("%1–%2").arg(start).arg(prev));
    };
    for (int i = 1; i < verses.size(); ++i) {
        if (verses.at(i) == prev + 1) {
            prev = verses.at(i);
            continue;
        }
        flush();
        start = prev = verses.at(i);
    }
    flush();

    return chapterRef + ',' + parts.join('.');
}

QJsonObject BiblePassage::toJson() const
{
    QJsonArray verseArray, textArray;
    for (int v : verses) verseArray.append(v);
    for (const QString &t : texts) textArray.append(t);

    return {
        {"bible", bibleId},
        {"abbreviation", abbreviation},
        {"bookName", bookName},
        {"book", book},
        {"chapter", chapter},
        {"verses", verseArray},
        {"texts", textArray},
        {"wholeChapter", wholeChapter},
        {"oneVersePerSlide", oneVersePerSlide},
    };
}

BiblePassage BiblePassage::fromJson(const QJsonObject &json)
{
    BiblePassage p;
    p.bibleId = json.value("bible").toString();
    p.abbreviation = json.value("abbreviation").toString();
    p.bookName = json.value("bookName").toString();
    p.book = json.value("book").toInt();
    p.chapter = json.value("chapter").toInt();
    for (const QJsonValue &v : json.value("verses").toArray()) p.verses << v.toInt();
    for (const QJsonValue &t : json.value("texts").toArray()) p.texts << t.toString();
    p.wholeChapter = json.value("wholeChapter").toBool();
    p.oneVersePerSlide = json.value("oneVersePerSlide").toBool();
    return p;
}
