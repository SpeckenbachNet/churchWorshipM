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
#include "textmarkup.h"

#include <QList>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>

namespace {

struct Style {
    bool bold = false;
    bool italic = false;
    bool underline = false;
};

struct Run {
    QString text;
    Style   style;
};

// Recognizes "<b>", "</i>", ... at 'pos'; returns the length of the tag (0 = no tag)
int parseTag(const QString &s, int pos, Style *style)
{
    const bool closing = pos + 1 < s.size() && s.at(pos + 1) == QLatin1Char('/');
    const int letter = pos + (closing ? 2 : 1);
    const int end = letter + 1;
    if (end >= s.size() || s.at(end) != QLatin1Char('>')) {
        return 0;
    }
    switch (s.at(letter).toLower().unicode()) {
    case 'b': style->bold = !closing; break;
    case 'i': style->italic = !closing; break;
    case 'u': style->underline = !closing; break;
    default:  return 0;
    }
    return end - pos + 1;
}

// Recognizes "&amp;" "&lt;" "&gt;" at 'pos'; returns the length (0 = none) and the character
int parseEntity(const QString &s, int pos, QChar *c)
{
    static const struct { const char *name; char c; } entities[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}};
    for (const auto &e : entities) {
        const QLatin1String name(e.name);
        if (QStringView(s).mid(pos).startsWith(name)) {
            *c = QLatin1Char(e.c);
            return int(name.size());
        }
    }
    return 0;
}

// One line of markup as runs of equally formatted text
QList<Run> parseLine(const QString &line)
{
    QList<Run> runs;
    Style style;
    QString text;
    auto flush = [&] {
        if (!text.isEmpty()) {
            runs.append({text, style});
            text.clear();
        }
    };
    for (int i = 0; i < line.size();) {
        const QChar c = line.at(i);
        if (c == QLatin1Char('<')) {
            Style next = style;
            if (const int len = parseTag(line, i, &next)) {
                flush();
                style = next;
                i += len;
                continue;
            }
        } else if (c == QLatin1Char('&')) {
            QChar decoded;
            if (const int len = parseEntity(line, i, &decoded)) {
                text += decoded;
                i += len;
                continue;
            }
        }
        text += c;
        ++i;
    }
    flush();
    return runs;
}

QString escaped(const QString &text)
{
    QString s = text;
    s.replace(QLatin1Char('&'), QLatin1String("&amp;"));
    s.replace(QLatin1Char('<'), QLatin1String("&lt;"));
    s.replace(QLatin1Char('>'), QLatin1String("&gt;"));
    return s;
}

QString withTags(const QString &text, const Style &style)
{
    QString s = escaped(text);
    if (style.underline) {
        s = QLatin1String("<u>") + s + QLatin1String("</u>");
    }
    if (style.italic) {
        s = QLatin1String("<i>") + s + QLatin1String("</i>");
    }
    if (style.bold) {
        s = QLatin1String("<b>") + s + QLatin1String("</b>");
    }
    return s;
}

} // namespace

namespace TextMarkup {

QString toPlain(const QString &markup)
{
    QStringList lines;
    for (const QString &line : markup.split(QLatin1Char('\n'))) {
        QString plain;
        for (const Run &run : parseLine(line)) {
            plain += run.text;
        }
        lines << plain;
    }
    return lines.join(QLatin1Char('\n'));
}

void fillDocument(QTextDocument *doc, const QString &markup, const QTextCharFormat &base)
{
    doc->clear();
    QTextCursor cursor(doc);
    cursor.setCharFormat(base);
    const QStringList lines = markup.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        if (i > 0) {
            cursor.insertBlock(cursor.blockFormat(), base);
        }
        for (const Run &run : parseLine(lines.at(i))) {
            QTextCharFormat format = base;
            format.setFontWeight(run.style.bold ? QFont::Bold : QFont::Normal);
            format.setFontItalic(run.style.italic);
            format.setFontUnderline(run.style.underline);
            cursor.insertText(run.text, format);
        }
    }
}

QString fromDocument(const QTextDocument *doc)
{
    QStringList lines;
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        QString line;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat f = fragment.charFormat();
            const Style style{f.fontWeight() >= QFont::DemiBold, f.fontItalic(), f.fontUnderline()};
            // Shift+Return inside a block counts as a new line as well
            const QStringList parts = fragment.text().split(QChar::LineSeparator);
            for (int i = 0; i < parts.size(); ++i) {
                if (i > 0) {
                    lines << line;
                    line.clear();
                }
                if (!parts.at(i).isEmpty()) {
                    line += withTags(parts.at(i), style);
                }
            }
        }
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace TextMarkup
