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
#include "swordimporter.h"
#include "biblebooks.h"
#include "biblestore.h"

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QtEndian>

// Qt has no public ZIP API; QZipReader is private but shipped with every Qt 6 installation
#include <QtCore/private/qzipreader_p.h>

namespace {

// ---------------------------------------------------------------------------
// Module configuration (mods.d/<name>.conf)
// ---------------------------------------------------------------------------

struct ModuleConf {
    QString name;                  // [GerNeUe]
    QHash<QString, QString> values;

    QString value(const QString &key, const QString &fallback = {}) const
    {
        return values.value(key, fallback);
    }
};

ModuleConf parseConf(const QByteArray &data)
{
    ModuleConf conf;
    QString pendingKey;
    const QStringList lines = QString::fromUtf8(data).split('\n');
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            conf.name = line.mid(1, line.size() - 2);
            continue;
        }
        // A trailing backslash continues the value on the next line
        if (!pendingKey.isEmpty()) {
            const bool more = line.endsWith('\\');
            conf.values[pendingKey] += '\n' + (more ? line.chopped(1) : line);
            if (!more) pendingKey.clear();
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0 || line.startsWith('#')) {
            continue;
        }
        const QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        if (value.endsWith('\\')) {
            value.chop(1);
            pendingKey = key;
        }
        // Some keys may appear several times (e.g. Feature); first one wins
        if (!conf.values.contains(key)) {
            conf.values.insert(key, value);
        }
    }
    return conf;
}

struct ConfFile {
    QString    root;   // path prefix of the module inside the ZIP ("" or "offbileModule/")
    ModuleConf conf;
};

QList<ConfFile> findConfs(QZipReader &zip)
{
    QList<ConfFile> confs;
    for (const QZipReader::FileInfo &fi : zip.fileInfoList()) {
        const qsizetype pos = fi.filePath.indexOf(QLatin1String("mods.d/"));
        if (fi.isFile && pos >= 0 && fi.filePath.endsWith(QLatin1String(".conf"), Qt::CaseInsensitive)) {
            confs << ConfFile{fi.filePath.left(pos), parseConf(zip.fileData(fi.filePath))};
        }
    }
    return confs;
}

// RTF-ish markup used in About/Copyright fields -> plain text
QString confText(QString text)
{
    text.replace(QLatin1String("\\par"), QLatin1String("\n"));
    text.replace(QLatin1String("\\pard"), QString());
    return text.trimmed();
}

// ---------------------------------------------------------------------------
// zText: compressed blocks (.bzz), block index (.bzs), verse index (.bzv)
// ---------------------------------------------------------------------------

class ZTextTestament {
public:
    bool load(const QByteArray &bzs, const QByteArray &bzv, const QByteArray &bzz)
    {
        m_bzs = bzs;
        m_bzv = bzv;
        m_bzz = bzz;
        return m_bzs.size() % 12 == 0 && m_bzv.size() % 10 == 0;
    }

    int entryCount() const { return int(m_bzv.size() / 10); }

    // Raw bytes of one index entry (verse, chapter/book heading, ...)
    QByteArray entry(int index)
    {
        if (index < 0 || index >= entryCount()) {
            return {};
        }
        const auto *v = reinterpret_cast<const uchar *>(m_bzv.constData()) + index * 10;
        const quint32 block = qFromLittleEndian<quint32>(v);
        const quint32 start = qFromLittleEndian<quint32>(v + 4);
        const quint16 size  = qFromLittleEndian<quint16>(v + 8);
        if (size == 0) {
            return {};
        }
        const QByteArray &data = blockData(block);
        if (qsizetype(start) + size > data.size()) {
            return {};
        }
        return data.mid(start, size);
    }

private:
    const QByteArray &blockData(quint32 block)
    {
        if (block != m_cachedBlock) {
            m_cached.clear();
            m_cachedBlock = block;
            if (qsizetype(block) * 12 + 12 <= m_bzs.size()) {
                const auto *b = reinterpret_cast<const uchar *>(m_bzs.constData()) + block * 12;
                const quint32 offset = qFromLittleEndian<quint32>(b);
                const quint32 csize  = qFromLittleEndian<quint32>(b + 4);
                const quint32 usize  = qFromLittleEndian<quint32>(b + 8);
                if (qsizetype(offset) + csize <= m_bzz.size()) {
                    // qUncompress expects the uncompressed size as 4 byte big endian prefix
                    QByteArray packed(4, Qt::Uninitialized);
                    qToBigEndian<quint32>(usize, packed.data());
                    packed.append(m_bzz.constData() + offset, csize);
                    m_cached = qUncompress(packed);
                }
            }
        }
        return m_cached;
    }

    QByteArray m_bzs, m_bzv, m_bzz;
    QByteArray m_cached;
    quint32    m_cachedBlock = 0xffffffff;
};

// ---------------------------------------------------------------------------
// Markup
// ---------------------------------------------------------------------------

QString decodeEntities(QString text)
{
    static const QRegularExpression numeric(QStringLiteral("&#(x?)([0-9a-fA-F]+);"));
    QRegularExpressionMatch m;
    qsizetype pos = 0;
    while ((m = numeric.match(text, pos)).hasMatch()) {
        bool ok = false;
        const uint code = m.captured(2).toUInt(&ok, m.captured(1).isEmpty() ? 10 : 16);
        const QString ch = ok ? QString::fromUcs4(reinterpret_cast<const char32_t *>(&code), 1) : QString();
        text.replace(m.capturedStart(), m.capturedLength(), ch);
        pos = m.capturedStart() + ch.size();
    }
    text.replace(QLatin1String("&lt;"), QLatin1String("<"));
    text.replace(QLatin1String("&gt;"), QLatin1String(">"));
    text.replace(QLatin1String("&quot;"), QLatin1String("\""));
    text.replace(QLatin1String("&apos;"), QLatin1String("'"));
    text.replace(QLatin1String("&nbsp;"), QString(QChar(0x00a0)));
    text.replace(QLatin1String("&amp;"), QLatin1String("&"));
    return text;
}

QString stripTags(const QString &text)
{
    static const QRegularExpression tag(QStringLiteral("<[^>]*>"));
    return QString(text).remove(tag);
}

} // namespace

namespace SwordImporter {

QString cleanVerseMarkup(const QString &raw, QString *heading)
{
    QString text = raw;

    // Footnotes / cross references: remove completely (they may contain <hi>, <reference>)
    static const QRegularExpression notes(QStringLiteral("<note\\b[^>]*/>|<note\\b[^>]*>.*?</note>"),
                                          QRegularExpression::DotMatchesEverythingOption);
    text.remove(notes);
    // ThML footnotes
    static const QRegularExpression thmlNotes(QStringLiteral("<scripRef\\b[^>]*>.*?</scripRef>"),
                                              QRegularExpression::DotMatchesEverythingOption);
    text.remove(thmlNotes);

    // Titles: editorial section titles -> heading, canonical titles (psalm superscriptions) stay in the text
    static const QRegularExpression titles(QStringLiteral("<title\\b([^>]*)>(.*?)</title>"),
                                           QRegularExpression::DotMatchesEverythingOption);
    QStringList headings;
    QRegularExpressionMatch m;
    while ((m = titles.match(text)).hasMatch()) {
        const QString inner = BibleText::normalize(decodeEntities(stripTags(m.captured(2))));
        QString replacement;
        if (m.captured(1).contains(QLatin1String("canonical=\"true\""))) {
            replacement = inner + '\n';
        } else if (!inner.isEmpty()) {
            headings << inner;
        }
        text.replace(m.capturedStart(), m.capturedLength(), replacement);
    }
    if (heading) {
        *heading = headings.join('\n');
    }

    // Line breaks of poetry
    static const QRegularExpression lineBreaks(QStringLiteral("<lb\\s*/>|<l\\b[^>]*eID=[^>]*/>|</l>|<br\\s*/?>"));
    text.replace(lineBreaks, QStringLiteral("\n"));

    return BibleText::normalize(decodeEntities(stripTags(text)));
}

QStringList modulesInZip(const QString &zipPath)
{
    QZipReader zip(zipPath);
    QStringList names;
    for (const ConfFile &cf : findConfs(zip)) {
        names << cf.conf.name;
    }
    return names;
}

QString importZip(const QString &zipPath, const BibleInfo &hint, const QString &module, QString *error)
{
    QZipReader zip(zipPath);
    if (!zip.isReadable()) {
        *error = QCoreApplication::translate("SwordImporter", "The file cannot be read.");
        return {};
    }

    // --- 1. Configuration
    const QList<ConfFile> confs = findConfs(zip);
    const ConfFile *selected = nullptr;
    for (const ConfFile &cf : confs) {
        if (module.isEmpty() || cf.conf.name.compare(module, Qt::CaseInsensitive) == 0) {
            selected = &cf;
            break;
        }
    }
    if (!selected) {
        *error = confs.isEmpty()
            ? QCoreApplication::translate("SwordImporter", "This is not a SWORD module (mods.d/*.conf missing).")
            : QCoreApplication::translate("SwordImporter", "The SWORD module %1 is not contained in the file.").arg(module);
        return {};
    }
    const ModuleConf &conf = selected->conf;

    if (conf.value("ModDrv").compare(QLatin1String("zText"), Qt::CaseInsensitive) != 0) {
        *error = QCoreApplication::translate("SwordImporter", "Unsupported SWORD module type: %1").arg(conf.value("ModDrv"));
        return {};
    }
    if (!conf.value("CipherKey").isNull()) {
        *error = QCoreApplication::translate("SwordImporter", "Encrypted SWORD modules are not supported.");
        return {};
    }
    const QString compress = conf.value("CompressType", "ZIP");
    if (compress.compare(QLatin1String("ZIP"), Qt::CaseInsensitive) != 0) {
        *error = QCoreApplication::translate("SwordImporter", "Unsupported compression: %1").arg(compress);
        return {};
    }
    const QString v11nName = conf.value("Versification", "KJV");
    const Versification *v11n = Versification::get(v11nName);
    if (!v11n) {
        *error = QCoreApplication::translate("SwordImporter", "Unsupported versification: %1").arg(v11nName);
        return {};
    }
    const bool utf8 = conf.value("Encoding").compare(QLatin1String("UTF-8"), Qt::CaseInsensitive) == 0;

    // DataPath is relative to the module root (the folder containing mods.d/)
    QString dataPath = conf.value("DataPath");
    if (dataPath.startsWith(QLatin1String("./"))) dataPath = dataPath.mid(2);
    if (!dataPath.endsWith('/')) dataPath += '/';
    dataPath = selected->root + dataPath;

    // --- 2. Target
    BibleInfo info;
    info.id = hint.id.isEmpty() ? conf.name.toLower() : hint.id;
    info.name = hint.name.isEmpty() ? conf.value("Description", conf.name) : hint.name;
    info.abbreviation = hint.abbreviation.isEmpty() ? conf.value("Abbreviation", conf.name) : hint.abbreviation;
    info.language = hint.language.isEmpty() ? conf.value("Lang") : hint.language;
    info.license = hint.license.isEmpty() ? conf.value("DistributionLicense") : hint.license;
    info.copyright = hint.copyright.isEmpty() ? confText(conf.value("CopyrightHolder", conf.value("Copyright")))
                                              : hint.copyright;
    info.source = hint.source;

    BibleWriter writer;
    if (!writer.begin(info, error)) {
        return {};
    }
    const bool german = info.language.startsWith(QLatin1String("de"));

    // --- 3. Verses. Layout of each testament index (see SWORD VersificationMgr):
    //   [0] module heading, [1] testament heading,
    //   per book: book heading, per chapter: chapter heading + one entry per verse
    const QList<QPair<QString, const QList<Versification::Book> *>> testaments = {
        {"ot", &v11n->ot}, {"nt", &v11n->nt},
    };
    for (const auto &[prefix, books] : testaments) {
        ZTextTestament testament;
        const QByteArray bzs = zip.fileData(dataPath + prefix + ".bzs");
        if (bzs.isEmpty()) {
            continue;   // e.g. NT-only modules
        }
        if (!testament.load(bzs, zip.fileData(dataPath + prefix + ".bzv"),
                            zip.fileData(dataPath + prefix + ".bzz"))) {
            *error = QCoreApplication::translate("SwordImporter", "The SWORD module is damaged.");
            return {};
        }

        int index = 1;
        for (const Versification::Book &book : *books) {
            ++index;   // book heading
            const int nr = BibleBooks::numberFromOsis(book.osisId);
            bool bookHasText = false;

            for (int chapter = 1; chapter <= book.verseCounts.size(); ++chapter) {
                ++index;   // chapter heading
                for (int verse = 1; verse <= book.verseCounts.at(chapter - 1); ++verse) {
                    const QByteArray raw = testament.entry(index + verse);
                    if (nr == 0 || raw.isEmpty()) {
                        continue;
                    }
                    QString heading;
                    const QString text = cleanVerseMarkup(utf8 ? QString::fromUtf8(raw)
                                                                : QString::fromLatin1(raw), &heading);
                    if (!text.isEmpty()) {
                        writer.addVerse(nr, chapter, verse, text, heading);
                        bookHasText = true;
                    }
                }
                index += book.verseCounts.at(chapter - 1);
            }

            if (bookHasText) {
                const QString name = german ? BibleBooks::germanName(nr) : book.osisId;
                writer.addBook(nr, name, int(book.verseCounts.size()));
            }
        }

        if (index + 1 != testament.entryCount()) {
            // Index size does not match the versification -> verses would be shifted
            *error = QCoreApplication::translate("SwordImporter", "The SWORD module does not match its versification (%1).").arg(v11nName);
            return {};
        }
    }

    if (!writer.commit(error)) {
        return {};
    }
    return info.id;
}

} // namespace SwordImporter
