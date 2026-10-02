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

#include <QString>

class QTextDocument;
class QTextCharFormat;

// Text of slides with simple formatting: bold, italic, underline.
//
// Stored as plain lines with the inline tags <b> <i> <u> (and their closing tags);
// '&', '<' and '>' of the text are written as &amp; &lt; &gt;. Every line is complete in
// itself (tags are closed at the end of the line), so splitting the text into slides at
// empty lines or "---" never cuts a tag in two.
// Plain text without tags is valid markup: an old text like "Gott & Mensch" stays as it is,
// because '&' and '<' only count as markup when they form one of the tags / entities above.
namespace TextMarkup {

QString toPlain(const QString &markup);   // without formatting, e.g. for search and titles
QString fromPlain(const QString &text);   // exactly this text, unformatted ("<b>" stays visible)

// Replaces the content of 'doc' by the text; every line becomes a block.
// 'base' is the format of unformatted text (font, color).
void fillDocument(QTextDocument *doc, const QString &markup, const QTextCharFormat &base);

// Bold / italic / underline of 'doc' as markup; everything else (fonts, colors, ...) is dropped
QString fromDocument(const QTextDocument *doc);

} // namespace TextMarkup
