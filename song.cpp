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
#include "song.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <iterator>

namespace {

struct KindInfo {
    SongPart::Kind kind;
    const char    *key;      // stored in JSON
    const char    *prefix;   // part id prefix
};

const KindInfo kKinds[] = {
    {SongPart::Verse,     "verse",     "V"},
    {SongPart::PreChorus, "prechorus", "P"},
    {SongPart::Chorus,    "chorus",    "C"},
    {SongPart::Bridge,    "bridge",    "B"},
    {SongPart::Tag,       "tag",       "T"},
    {SongPart::Intro,     "intro",     "I"},
    {SongPart::Interlude, "interlude", "Z"},
    {SongPart::Ending,    "ending",    "E"},
    {SongPart::Other,     "other",     "X"},
};

const KindInfo &kindInfo(SongPart::Kind kind)
{
    for (const KindInfo &k : kKinds) {
        if (k.kind == kind) {
            return k;
        }
    }
    return kKinds[std::size(kKinds) - 1];
}

SongPart::Kind kindFromKey(const QString &key)
{
    for (const KindInfo &k : kKinds) {
        if (key == QLatin1String(k.key)) {
            return k.kind;
        }
    }
    return SongPart::Other;
}

// Section headers as written by SongSelect (always English) and common German names
bool parseHeader(const QString &line, SongPart::Kind *kind, int *number)
{
    static const QRegularExpression re(
        QStringLiteral("^(verse|vers|strophe|pre-?chorus|chorus|refrain|bridge|tag|intro|"
                       "interlude|zwischenspiel|ending|outro|schluss|misc)\\s*(\\d+)?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(line.trimmed());
    if (!m.hasMatch()) {
        return false;
    }
    const QString word = m.captured(1).toLower().remove('-');
    if (word == "verse" || word == "vers" || word == "strophe") {
        *kind = SongPart::Verse;
    } else if (word == "prechorus") {
        *kind = SongPart::PreChorus;
    } else if (word == "chorus" || word == "refrain") {
        *kind = SongPart::Chorus;
    } else if (word == "bridge") {
        *kind = SongPart::Bridge;
    } else if (word == "tag") {
        *kind = SongPart::Tag;
    } else if (word == "intro") {
        *kind = SongPart::Intro;
    } else if (word == "interlude" || word == "zwischenspiel") {
        *kind = SongPart::Interlude;
    } else if (word == "ending" || word == "outro" || word == "schluss") {
        *kind = SongPart::Ending;
    } else {
        *kind = SongPart::Other;
    }
    *number = m.captured(2).toInt();
    return true;
}

bool isFooter(const QStringList &block)
{
    for (const QString &line : block) {
        if (line.contains(QLatin1String("CCLI"), Qt::CaseInsensitive) || line.startsWith(QChar(0x00A9))) {
            return true;
        }
    }
    return false;
}

QString digits(const QString &line)
{
    static const QRegularExpression re(QStringLiteral("(\\d{3,})"));
    return re.match(line).captured(1);
}

} // namespace

// ---------------------------------------------------------------------------
// SongPart
// ---------------------------------------------------------------------------

QString SongPart::label() const
{
    QString name;
    switch (kind) {
    case Verse:     name = QCoreApplication::translate("SongPart", "Verse"); break;
    case PreChorus: name = QCoreApplication::translate("SongPart", "Pre-Chorus"); break;
    case Chorus:    name = QCoreApplication::translate("SongPart", "Chorus"); break;
    case Bridge:    name = QCoreApplication::translate("SongPart", "Bridge"); break;
    case Tag:       name = QCoreApplication::translate("SongPart", "Tag"); break;
    case Intro:     name = QCoreApplication::translate("SongPart", "Intro"); break;
    case Interlude: name = QCoreApplication::translate("SongPart", "Interlude"); break;
    case Ending:    name = QCoreApplication::translate("SongPart", "Ending"); break;
    case Other:     name = QCoreApplication::translate("SongPart", "Part"); break;
    }
    return number > 0 ? QStringLiteral("%1 %2").arg(name).arg(number) : name;
}

QString SongPart::idPrefix(Kind kind)
{
    return QLatin1String(kindInfo(kind).prefix);
}

QColor SongPart::color(Kind kind)
{
    switch (kind) {
    case Verse:     return QColor(0x3b, 0x82, 0xf6);   // blue
    case PreChorus: return QColor(0xd9, 0x77, 0x06);   // amber
    case Chorus:    return QColor(0xea, 0x58, 0x0c);   // orange
    case Bridge:    return QColor(0x8b, 0x5c, 0xf6);   // violet
    case Tag:       return QColor(0x0d, 0x94, 0x88);   // teal
    case Intro:
    case Interlude:
    case Ending:    return QColor(0x16, 0xa3, 0x4a);   // green
    case Other:     break;
    }
    return QColor(0x6b, 0x72, 0x80);   // gray
}

QJsonObject SongPart::toJson() const
{
    QJsonObject o{{"id", id}, {"kind", QLatin1String(kindInfo(kind).key)}, {"text", text}};
    if (number > 0) {
        o.insert("number", number);
    }
    return o;
}

SongPart SongPart::fromJson(const QJsonObject &o)
{
    SongPart p;
    p.id = o.value("id").toString();
    p.kind = kindFromKey(o.value("kind").toString());
    p.number = o.value("number").toInt();
    p.text = o.value("text").toString();
    return p;
}

// ---------------------------------------------------------------------------
// Song
// ---------------------------------------------------------------------------

int Song::partIndex(const QString &partId) const
{
    for (int i = 0; i < parts.size(); ++i) {
        if (parts.at(i).id == partId) {
            return i;
        }
    }
    return -1;
}

QList<SongPart> Song::arrangedParts(const QStringList &orderOverride) const
{
    const QStringList &ids = !orderOverride.isEmpty() ? orderOverride : order;
    if (ids.isEmpty()) {
        return parts;
    }
    QList<SongPart> result;
    for (const QString &partId : ids) {
        const int i = partIndex(partId);
        if (i >= 0) {
            result << parts.at(i);
        }
    }
    return result;
}

namespace {

// Slides of one part: a line "---" splits, and so does an empty line (like the text slides)
QStringList partSlides(const QString &text)
{
    static const QRegularExpression split(QStringLiteral(R"(\n\s*-{3,}\s*(\n|$)|\n\s*\n)"));
    QStringList slides;
    for (const QString &slide : text.split(split, Qt::SkipEmptyParts)) {
        if (!slide.trimmed().isEmpty()) {
            slides << slide.trimmed();
        }
    }
    return slides;
}

} // namespace

QString Song::slideText(const QStringList &orderOverride) const
{
    QStringList slides;
    for (const SongPart &part : arrangedParts(orderOverride)) {
        slides << partSlides(part.text);
    }
    return slides.join(QLatin1String("\n\n"));
}

QList<SongPart> Song::slideParts(const QStringList &orderOverride) const
{
    QList<SongPart> result;
    for (const SongPart &part : arrangedParts(orderOverride)) {
        for (qsizetype i = partSlides(part.text).size(); i > 0; --i) {
            result << part;
        }
    }
    return result;
}

QString Song::allText() const
{
    QStringList texts;
    for (const SongPart &p : parts) {
        texts << p.text;
    }
    return texts.join('\n');
}

QJsonObject Song::toJson() const
{
    QJsonArray partArray;
    for (const SongPart &p : parts) {
        partArray.append(p.toJson());
    }
    QJsonObject o{{"title", title}, {"parts", partArray}, {"order", QJsonArray::fromStringList(order)}};
    if (!authors.isEmpty()) {
        o.insert("authors", authors);
    }
    if (!copyright.isEmpty()) {
        o.insert("copyright", copyright);
    }
    if (!ccliNumber.isEmpty()) {
        o.insert("ccli", ccliNumber);
    }
    return o;
}

Song Song::fromJson(const QJsonObject &o)
{
    Song s;
    s.title = o.value("title").toString();
    s.authors = o.value("authors").toString();
    s.copyright = o.value("copyright").toString();
    s.ccliNumber = o.value("ccli").toString();
    for (const QJsonValue &v : o.value("parts").toArray()) {
        s.parts << SongPart::fromJson(v.toObject());
    }
    for (const QJsonValue &v : o.value("order").toArray()) {
        s.order << v.toString();
    }
    return s;
}

Song Song::fromSongSelectText(const QString &text, QString *licence)
{
    // Blocks separated by empty lines: title, parts (header line + lyrics), footer
    QString normalized = text;
    normalized.remove(QChar(0xFEFF));
    normalized.replace(QLatin1String("\r\n"), QLatin1String("\n")).replace('\r', '\n');

    QList<QStringList> blocks;
    QStringList current;
    for (const QString &raw : normalized.split('\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            if (!current.isEmpty()) {
                blocks << current;
                current.clear();
            }
        } else {
            current << line;
        }
    }
    if (!current.isEmpty()) {
        blocks << current;
    }

    Song song;
    if (blocks.isEmpty()) {
        return song;
    }
    song.title = blocks.takeFirst().join(' ');

    for (const QStringList &block : std::as_const(blocks)) {
        if (isFooter(block)) {
            for (const QString &line : block) {
                if (line.startsWith(QChar(0x00A9))) {
                    song.copyright = line.mid(1).trimmed();
                } else if (line.contains(QLatin1String("ccli.com"), Qt::CaseInsensitive)) {
                    continue;   // usage notice
                } else if (line.contains(QLatin1String("Lizenz"), Qt::CaseInsensitive)
                           || line.contains(QLatin1String("Licen"), Qt::CaseInsensitive)) {
                    if (licence) {
                        *licence = digits(line);
                    }
                } else if (line.contains(QLatin1String("CCLI"), Qt::CaseInsensitive)) {
                    song.ccliNumber = digits(line);
                } else if (song.authors.isEmpty()) {
                    song.authors = line;
                }
            }
            continue;
        }

        SongPart part;
        QStringList lines = block;
        if (parseHeader(lines.first(), &part.kind, &part.number)) {
            lines.removeFirst();
        }
        if (lines.isEmpty()) {
            continue;
        }
        part.text = lines.join('\n');

        // Unique id: "Chorus" twice becomes C and C-2
        const QString base = QLatin1String(kindInfo(part.kind).prefix)
                             + (part.number > 0 ? QString::number(part.number) : QString());
        part.id = base;
        for (int n = 2; song.partIndex(part.id) >= 0; ++n) {
            part.id = QStringLiteral("%1-%2").arg(base).arg(n);
        }
        song.parts << part;
        song.order << part.id;   // SongSelect has no order: sung as listed until arranged
    }
    return song;
}
