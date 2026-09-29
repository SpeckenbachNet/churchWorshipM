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
#include "bibleimporter.h"
#include "biblebooks.h"
#include "swordimporter.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QXmlStreamReader>

#include <QtCore/private/qzipreader_p.h>

namespace {

QString fromHint(const QString &hint, const QString &fallback)
{
    return hint.isEmpty() ? fallback : hint;
}

bool isGermanLanguage(const QString &lang)
{
    const QString l = lang.toLower();
    return l.startsWith(QLatin1String("de")) || l.startsWith(QLatin1String("ger"));
}

QString bookName(int nr, const QString &sourceName, bool german)
{
    if (!sourceName.trimmed().isEmpty()) {
        return sourceName.trimmed();
    }
    return german ? BibleBooks::germanName(nr) : BibleBooks::osisId(nr);
}

bool nameIs(const QXmlStreamReader &xml, const char *name)
{
    return xml.name().compare(QLatin1String(name), Qt::CaseInsensitive) == 0;
}

// Collects the verses of one import and writes books once all verses are known
class Collector {
public:
    Collector(BibleWriter &writer, bool german) : m_writer(writer), m_german(german) {}

    void setBookName(int nr, const QString &name) { m_names[nr] = name; }

    void addVerse(int book, int chapter, int verse, const QString &rawText, const QString &heading)
    {
        const QString text = BibleText::normalize(rawText);
        if (book <= 0 || chapter <= 0 || verse <= 0 || text.isEmpty()) {
            return;
        }
        m_writer.addVerse(book, chapter, verse, text, BibleText::normalize(heading));
        m_chapters[book] = qMax(m_chapters.value(book), chapter);
    }

    void writeBooks()
    {
        for (auto it = m_chapters.cbegin(); it != m_chapters.cend(); ++it) {
            m_writer.addBook(it.key(), bookName(it.key(), m_names.value(it.key()), m_german), it.value());
        }
    }

private:
    BibleWriter     &m_writer;
    bool             m_german;
    QMap<int, int>     m_chapters;
    QMap<int, QString> m_names;
};

QString xmlError(const QXmlStreamReader &xml)
{
    return QCoreApplication::translate("BibleImporter", "Invalid XML (line %1): %2")
        .arg(xml.lineNumber()).arg(xml.errorString());
}

// "Gen.1.1" or "Gen.1.1-Gen.1.3" or "Gen.1.1 Gen.1.2" -> first reference
bool parseOsisRef(const QString &ref, int *book, int *chapter, int *verse)
{
    const QString first = ref.section(' ', 0, 0).section('-', 0, 0);
    const QStringList parts = first.split('.');
    if (parts.size() < 3) {
        return false;
    }
    *book = BibleBooks::numberFromOsis(parts.at(0));
    static const QRegularExpression digits(QStringLiteral("^\\d+"));
    *chapter = digits.match(parts.at(1)).captured(0).toInt();
    *verse = digits.match(parts.at(2)).captured(0).toInt();
    return *book > 0;
}

} // namespace

namespace BibleImporter {

QString idFromFileName(const QString &path)
{
    static const QRegularExpression invalid(QStringLiteral("[^a-z0-9_-]+"));
    QString id = QFileInfo(path).completeBaseName().toLower().replace(invalid, QStringLiteral("_"));
    return id.isEmpty() ? QStringLiteral("bible") : id;
}

// ---------------------------------------------------------------------------
// getBible.net v2 JSON
// ---------------------------------------------------------------------------

QString importGetBible(const QString &path, const BibleInfo &hint, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("BibleImporter", "The file cannot be read.");
        return {};
    }
    QJsonParseError parseError;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll(), &parseError).object();
    const QJsonArray books = root.value("books").toArray();
    if (parseError.error != QJsonParseError::NoError || books.isEmpty()) {
        *error = QCoreApplication::translate("BibleImporter", "This is not a getBible file.");
        return {};
    }

    BibleInfo info;
    info.id = fromHint(hint.id, idFromFileName(path));
    info.name = fromHint(hint.name, root.value("translation").toString());
    info.abbreviation = fromHint(hint.abbreviation, root.value("abbreviation").toString());
    info.language = fromHint(hint.language, root.value("lang").toString());
    info.license = fromHint(hint.license, root.value("distribution_license").toString());
    info.copyright = hint.copyright;
    info.source = hint.source;

    BibleWriter writer;
    if (!writer.begin(info, error)) {
        return {};
    }
    Collector collector(writer, isGermanLanguage(info.language));

    for (const QJsonValue &b : books) {
        const QJsonObject book = b.toObject();
        const int nr = book.value("nr").toInt();
        if (nr < 1 || nr > 66) {
            continue;   // getBible numbers only the protestant canon reliably
        }
        collector.setBookName(nr, book.value("name").toString());
        for (const QJsonValue &c : book.value("chapters").toArray()) {
            const QJsonObject chapter = c.toObject();
            for (const QJsonValue &v : chapter.value("verses").toArray()) {
                const QJsonObject verse = v.toObject();
                collector.addVerse(nr, verse.value("chapter").toInt(chapter.value("chapter").toInt()),
                                   verse.value("verse").toInt(), verse.value("text").toString(), {});
            }
        }
    }

    collector.writeBooks();
    return writer.commit(error) ? info.id : QString();
}

// ---------------------------------------------------------------------------
// OSIS (container verses <verse osisID>..</verse> and milestones <verse sID/> .. <verse eID/>)
// ---------------------------------------------------------------------------

QString importOsis(const QString &path, const BibleInfo &hint, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("BibleImporter", "The file cannot be read.");
        return {};
    }
    QXmlStreamReader xml(&file);

    BibleInfo info;
    info.copyright = hint.copyright;
    info.source = hint.source;
    QString headerTitle, headerRights, headerLanguage, textLanguage, workId;

    BibleWriter writer;
    std::unique_ptr<Collector> collector;

    bool inHeader = false;
    QString *headerField = nullptr;

    int skipDepth = 0;                 // inside <note> etc.
    bool inVerse = false;
    int book = 0, chapter = 0, verse = 0;
    QString verseText, verseHeading, pendingHeading;
    QList<bool> verseStack;            // true = container verse (needs its end tag)

    int titleDepth = 0;                // > 0 while inside <title>
    bool titleCanonical = false;
    QString titleText;

    auto flushVerse = [&] {
        if (inVerse && collector) {
            collector->addVerse(book, chapter, verse, verseText, verseHeading);
        }
        inVerse = false;
        verseText.clear();
        verseHeading.clear();
    };

    // The header has to be known before the first verse is written
    auto ensureStarted = [&]() -> bool {
        if (collector) {
            return true;
        }
        info.id = fromHint(hint.id, workId.isEmpty() ? idFromFileName(path) : workId.toLower());
        info.name = fromHint(hint.name, headerTitle.isEmpty() ? QFileInfo(path).completeBaseName() : headerTitle);
        info.abbreviation = fromHint(hint.abbreviation, workId);
        info.language = fromHint(hint.language, headerLanguage.trimmed().isEmpty() ? textLanguage
                                                                                   : headerLanguage.trimmed());
        info.license = fromHint(hint.license, headerRights);
        if (!writer.begin(info, error)) {
            return false;
        }
        collector = std::make_unique<Collector>(writer, isGermanLanguage(info.language));
        return true;
    };

    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement: {
            if (skipDepth > 0) {
                ++skipDepth;
                break;
            }
            if (titleDepth > 0) {
                ++titleDepth;
                break;
            }
            const QXmlStreamAttributes attrs = xml.attributes();

            if (nameIs(xml, "osisText")) {
                workId = attrs.value("osisIDWork").toString();
                textLanguage = attrs.value("xml:lang").toString();
            } else if (nameIs(xml, "header")) {
                inHeader = true;
            } else if (inHeader) {
                if (nameIs(xml, "title") && headerTitle.isEmpty())  headerField = &headerTitle;
                else if (nameIs(xml, "rights"))                    headerField = &headerRights;
                else if (nameIs(xml, "language") && headerLanguage.isEmpty()) headerField = &headerLanguage;
            } else if (nameIs(xml, "note")) {
                skipDepth = 1;
            } else if (nameIs(xml, "verse")) {
                if (!ensureStarted()) {
                    return {};
                }
                if (attrs.hasAttribute("eID")) {
                    flushVerse();
                    verseStack << false;
                    break;
                }
                flushVerse();
                const QString ref = attrs.hasAttribute("osisID") ? attrs.value("osisID").toString()
                                                                 : attrs.value("sID").toString();
                inVerse = parseOsisRef(ref, &book, &chapter, &verse);
                verseHeading = pendingHeading;
                pendingHeading.clear();
                verseStack << !attrs.hasAttribute("sID");
            } else if (nameIs(xml, "title")) {
                titleDepth = 1;
                titleText.clear();
                titleCanonical = attrs.value("canonical") == QLatin1String("true");
            } else if (nameIs(xml, "lb") || (nameIs(xml, "l") && attrs.hasAttribute("eID"))) {
                if (inVerse) verseText += '\n';
            }
            break;
        }
        case QXmlStreamReader::EndElement:
            if (skipDepth > 0) {
                --skipDepth;
                break;
            }
            if (titleDepth > 0) {
                if (--titleDepth == 0) {
                    const QString title = BibleText::normalize(titleText);
                    if (titleCanonical && inVerse) {
                        verseText += title + '\n';
                    } else if (!title.isEmpty()) {
                        QString &target = inVerse ? verseHeading : pendingHeading;
                        target += (target.isEmpty() ? QString() : QStringLiteral("\n")) + title;
                    }
                }
                break;
            }
            if (nameIs(xml, "header")) {
                inHeader = false;
            } else if (inHeader) {
                headerField = nullptr;
            } else if (nameIs(xml, "verse") && !verseStack.isEmpty()) {
                if (verseStack.takeLast()) {
                    flushVerse();   // end of a container verse
                }
            } else if (nameIs(xml, "l") && inVerse) {
                verseText += '\n';
            }
            break;
        case QXmlStreamReader::Characters:
            if (skipDepth > 0) {
                break;
            }
            if (inHeader) {
                if (headerField) *headerField += xml.text();
            } else if (titleDepth > 0) {
                titleText += xml.text();
            } else if (inVerse) {
                verseText += xml.text();
            }
            break;
        default:
            break;
        }
    }
    if (xml.hasError()) {
        *error = xmlError(xml);
        return {};
    }
    flushVerse();
    if (!collector) {
        *error = QCoreApplication::translate("BibleImporter", "The file contains no bible verses.");
        return {};
    }
    collector->writeBooks();
    return writer.commit(error) ? info.id : QString();
}

// ---------------------------------------------------------------------------
// Zefania XML
// ---------------------------------------------------------------------------

QString importZefania(const QString &path, const BibleInfo &hint, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("BibleImporter", "The file cannot be read.");
        return {};
    }
    QXmlStreamReader xml(&file);

    BibleInfo info;
    info.copyright = hint.copyright;
    info.source = hint.source;
    QString bibleName, infoTitle, infoIdentifier, infoLanguage, infoRights;

    BibleWriter writer;
    std::unique_ptr<Collector> collector;

    bool inInformation = false;
    QString *infoField = nullptr;
    int skipDepth = 0;                 // inside <NOTE>, <DIV>
    bool inVerse = false, inCaption = false;
    int book = 0, chapter = 0, verse = 0;
    QString verseText, captionText, pendingHeading;

    auto ensureStarted = [&]() -> bool {
        if (collector) {
            return true;
        }
        info.id = fromHint(hint.id, infoIdentifier.isEmpty() ? idFromFileName(path) : infoIdentifier.toLower());
        info.name = fromHint(hint.name, !infoTitle.isEmpty() ? infoTitle
                                        : !bibleName.isEmpty() ? bibleName
                                        : QFileInfo(path).completeBaseName());
        info.abbreviation = fromHint(hint.abbreviation, infoIdentifier);
        info.language = fromHint(hint.language, isGermanLanguage(infoLanguage) ? QStringLiteral("de") : infoLanguage);
        info.license = fromHint(hint.license, infoRights);
        if (!writer.begin(info, error)) {
            return false;
        }
        collector = std::make_unique<Collector>(writer, isGermanLanguage(info.language));
        return true;
    };

    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartElement: {
            if (skipDepth > 0) {
                ++skipDepth;
                break;
            }
            const QXmlStreamAttributes attrs = xml.attributes();
            if (nameIs(xml, "XMLBIBLE")) {
                bibleName = attrs.value("biblename").toString();
            } else if (nameIs(xml, "INFORMATION")) {
                inInformation = true;
            } else if (inInformation) {
                if (nameIs(xml, "title"))           infoField = &infoTitle;
                else if (nameIs(xml, "identifier")) infoField = &infoIdentifier;
                else if (nameIs(xml, "language"))   infoField = &infoLanguage;
                else if (nameIs(xml, "rights"))     infoField = &infoRights;
            } else if (nameIs(xml, "NOTE") || nameIs(xml, "DIV") || nameIs(xml, "REMARK")) {
                skipDepth = 1;
            } else if (nameIs(xml, "BIBLEBOOK")) {
                if (!ensureStarted()) {
                    return {};
                }
                const int nr = attrs.value("bnumber").toInt();
                book = (nr >= 1 && nr <= 66) ? nr : 0;   // apocrypha numbering differs between files
                if (book > 0) {
                    collector->setBookName(book, attrs.value("bname").toString());
                }
            } else if (nameIs(xml, "CHAPTER")) {
                chapter = attrs.value("cnumber").toInt();
            } else if (nameIs(xml, "CAPTION")) {
                inCaption = true;
                captionText.clear();
            } else if (nameIs(xml, "VERS")) {
                inVerse = true;
                verse = attrs.value("vnumber").toInt();
                verseText.clear();
            } else if (nameIs(xml, "BR") && inVerse) {
                verseText += '\n';
            }
            break;
        }
        case QXmlStreamReader::EndElement:
            if (skipDepth > 0) {
                --skipDepth;
                break;
            }
            if (nameIs(xml, "INFORMATION")) {
                inInformation = false;
            } else if (inInformation) {
                infoField = nullptr;
            } else if (nameIs(xml, "CAPTION")) {
                inCaption = false;
                pendingHeading += (pendingHeading.isEmpty() ? QString() : QStringLiteral("\n")) + captionText;
            } else if (nameIs(xml, "VERS") && inVerse) {
                inVerse = false;
                collector->addVerse(book, chapter, verse, verseText, pendingHeading);
                pendingHeading.clear();
            }
            break;
        case QXmlStreamReader::Characters:
            if (skipDepth > 0) {
                break;
            }
            if (inInformation) {
                if (infoField) *infoField += xml.text();
            } else if (inCaption) {
                captionText += xml.text();
            } else if (inVerse) {
                verseText += xml.text();
            }
            break;
        default:
            break;
        }
    }
    if (xml.hasError()) {
        *error = xmlError(xml);
        return {};
    }
    if (!collector) {
        *error = QCoreApplication::translate("BibleImporter", "The file contains no bible verses.");
        return {};
    }
    collector->writeBooks();
    return writer.commit(error) ? info.id : QString();
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

QString importFile(const QString &path, const BibleInfo &hint, const QString &swordModule, QString *error)
{
    const QString suffix = QFileInfo(path).suffix().toLower();

    if (suffix == QLatin1String("zip")) {
        if (!SwordImporter::modulesInZip(path).isEmpty()) {
            return SwordImporter::importZip(path, hint, swordModule, error);
        }
        // A zipped XML / JSON bible: extract the first one and import it
        QZipReader zip(path);
        for (const QZipReader::FileInfo &fi : zip.fileInfoList()) {
            const QString inner = QFileInfo(fi.filePath).suffix().toLower();
            if (!fi.isFile || (inner != QLatin1String("xml") && inner != QLatin1String("json"))) {
                continue;
            }
            QTemporaryDir tmp;
            const QString extracted = tmp.filePath(QFileInfo(fi.filePath).fileName());
            QFile out(extracted);
            if (!tmp.isValid() || !out.open(QIODevice::WriteOnly)) {
                *error = QCoreApplication::translate("BibleImporter", "Cannot create a temporary folder.");
                return {};
            }
            out.write(zip.fileData(fi.filePath));
            out.close();
            BibleInfo innerHint = hint;
            if (innerHint.id.isEmpty()) innerHint.id = idFromFileName(path);
            return importFile(extracted, innerHint, swordModule, error);
        }
        *error = QCoreApplication::translate("BibleImporter", "The ZIP file contains no known bible format.");
        return {};
    }

    if (suffix == QLatin1String("json")) {
        return importGetBible(path, hint, error);
    }

    // XML: decide by the root element
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QCoreApplication::translate("BibleImporter", "The file cannot be read.");
        return {};
    }
    QXmlStreamReader xml(&file);
    while (!xml.atEnd() && xml.readNext() != QXmlStreamReader::StartElement) {}
    file.close();

    if (nameIs(xml, "osis")) {
        return importOsis(path, hint, error);
    }
    if (nameIs(xml, "XMLBIBLE")) {
        return importZefania(path, hint, error);
    }
    *error = QCoreApplication::translate("BibleImporter", "Unknown bible format.");
    return {};
}

} // namespace BibleImporter
