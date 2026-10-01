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
#include "medialibrary.h"
#include "videothumbnailer.h"
#include "presentationconverter.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QPainter>
#include <QPdfDocument>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace {

const QString kConnection = QStringLiteral("medialibrary");
constexpr QSize kThumbSize{320, 180};

QString fileSha1(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}

qint64 mtimeOf(const QString &path)
{
    return QFileInfo(path).lastModified().toSecsSinceEpoch();
}

QImage renderPdfPage(const QString &pdfPath)
{
    QPdfDocument doc;
    if (doc.load(pdfPath) != QPdfDocument::Error::None || doc.pageCount() < 1) {
        return {};
    }
    const QSize size = doc.pagePointSize(0).scaled(kThumbSize, Qt::KeepAspectRatio).toSize();
    if (size.isEmpty()) {
        return {};
    }
    // Pages are rendered transparent; most PDFs have no own page background -> white paper
    QImage page(size, QImage::Format_RGB32);
    page.fill(Qt::white);
    QPainter p(&page);
    p.drawImage(0, 0, doc.render(0, size));
    return page;
}

} // namespace

// ---------------------------------------------------------------------------
// LibraryEntry
// ---------------------------------------------------------------------------

QString LibraryEntry::path() const
{
    return linked ? originalPath : MediaLibrary::filesDir() + "/" + fileName;
}

bool LibraryEntry::isMissing() const
{
    return !QFileInfo::exists(path());
}

bool LibraryEntry::updateAvailable() const
{
    return !linked && QFileInfo::exists(originalPath) && mtimeOf(originalPath) > originalMtime;
}

// ---------------------------------------------------------------------------
// MediaLibrary
// ---------------------------------------------------------------------------

QString MediaLibrary::dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/library";
}

QString MediaLibrary::filesDir()
{
    return dir() + "/files";
}

bool MediaLibrary::isSupported(const QString &path)
{
    const MediaItem::Type type = MediaItem::typeFromFile(path);
    return type == MediaItem::Image || type == MediaItem::Pdf || type == MediaItem::PowerPoint
           || type == MediaItem::Video;
}

MediaLibrary::MediaLibrary(PresentationConverter *converter, QObject *parent)
    : QObject(parent),
    m_converter(converter)
{
    QDir().mkpath(filesDir());

    m_videoThumbnailer = new VideoThumbnailer(this);
    connect(m_videoThumbnailer, &VideoThumbnailer::ready, this, [this](const QString &id, const QImage &image) {
        m_videoThumbsPending.remove(id);
        if (!image.isNull()) {
            image.scaled(kThumbSize, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(thumbPath(id), "PNG");
            emit thumbnailChanged(id);
        }
    });

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), kConnection);
    m_db.setDatabaseName(dir() + "/library.sqlite");
    if (m_db.open()) {
        QSqlQuery q(m_db);
        q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS media ("
                              "id TEXT PRIMARY KEY, title TEXT, type TEXT, linked INTEGER, file TEXT, "
                              "original TEXT, original_mtime INTEGER, sha1 TEXT, added TEXT)"));
        load();
    }

    // Presentation converted -> its thumbnail can be created now
    connect(m_converter, &PresentationConverter::finished, this,
            [this](const QString &source, const QString &error) {
        if (!error.isEmpty()) {
            return;
        }
        for (const LibraryEntry &e : std::as_const(m_entries)) {
            if (e.path() == source) {
                QFile::remove(thumbPath(e.id));
                emit thumbnailChanged(e.id);
            }
        }
    });

    // Convert presentations that are not in the cache yet (e.g. changed linked originals)
    for (const LibraryEntry &e : std::as_const(m_entries)) {
        if (e.type == MediaItem::PowerPoint && !e.isMissing()) {
            m_converter->convert(e.path());
        }
    }
}

MediaLibrary::~MediaLibrary()
{
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(kConnection);
}

void MediaLibrary::load()
{
    QSqlQuery q(QStringLiteral("SELECT id, title, type, linked, file, original, original_mtime, sha1, added "
                               "FROM media"), m_db);
    while (q.next()) {
        LibraryEntry e;
        e.id = q.value(0).toString();
        e.title = q.value(1).toString();
        e.type = MediaItem::typeFromKey(q.value(2).toString());
        e.linked = q.value(3).toBool();
        e.fileName = q.value(4).toString();
        e.originalPath = q.value(5).toString();
        e.originalMtime = q.value(6).toLongLong();
        e.sha1 = q.value(7).toString();
        e.added = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        m_entries.insert(e.id, e);
    }
}

bool MediaLibrary::save(const LibraryEntry &e)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("INSERT OR REPLACE INTO media "
                             "(id, title, type, linked, file, original, original_mtime, sha1, added) "
                             "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(e.id);
    q.addBindValue(e.title);
    q.addBindValue(MediaItem::typeKey(e.type));
    q.addBindValue(e.linked);
    q.addBindValue(e.fileName);
    q.addBindValue(e.originalPath);
    q.addBindValue(e.originalMtime);
    q.addBindValue(e.sha1);
    q.addBindValue(e.added.toString(Qt::ISODate));
    if (!q.exec()) {
        return false;
    }
    m_entries.insert(e.id, e);
    return true;
}

QList<LibraryEntry> MediaLibrary::entries() const
{
    QList<LibraryEntry> list = m_entries.values();
    std::sort(list.begin(), list.end(), [](const LibraryEntry &a, const LibraryEntry &b) {
        return a.title.localeAwareCompare(b.title) < 0;
    });
    return list;
}

LibraryEntry MediaLibrary::entry(const QString &id) const
{
    return m_entries.value(id);
}

void MediaLibrary::prepare(const LibraryEntry &e)
{
    QFile::remove(thumbPath(e.id));
    if (e.type == MediaItem::PowerPoint) {
        m_converter->convert(e.path());
    }
}

QString MediaLibrary::addFile(const QString &path, bool link, QString *error)
{
    const QFileInfo fi(path);
    if (!isSupported(path)) {
        *error = tr("Unsupported file type: %1").arg(fi.fileName());
        return {};
    }
    const QString original = fi.absoluteFilePath();
    const QString sha = fileSha1(original);
    if (sha.isEmpty()) {
        *error = tr("Cannot read %1.").arg(fi.fileName());
        return {};
    }

    // Already in the library? Copies are recognized by their content, links by their path.
    for (const LibraryEntry &e : std::as_const(m_entries)) {
        if (link ? (e.linked && e.originalPath == original) : (!e.linked && e.sha1 == sha)) {
            return e.id;
        }
    }

    LibraryEntry e;
    e.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    e.title = fi.completeBaseName();
    e.type = MediaItem::typeFromFile(original);
    e.linked = link;
    e.originalPath = original;
    e.originalMtime = mtimeOf(original);
    e.sha1 = sha;
    e.added = QDateTime::currentDateTime();
    if (!link) {
        e.fileName = e.id + "." + fi.suffix().toLower();
        if (!QFile::copy(original, e.path())) {
            *error = tr("%1 could not be copied into the media library.").arg(fi.fileName());
            return {};
        }
    }

    if (!save(e)) {
        QFile::remove(e.path());
        *error = m_db.lastError().text();
        return {};
    }
    prepare(e);
    emit changed();
    return e.id;
}

bool MediaLibrary::rename(const QString &id, const QString &title)
{
    LibraryEntry e = entry(id);
    if (!e.isValid() || title.trimmed().isEmpty() || title == e.title) {
        return false;
    }
    e.title = title.trimmed();
    if (!save(e)) {
        return false;
    }
    emit changed();
    return true;
}

bool MediaLibrary::remove(const QString &id)
{
    const LibraryEntry e = entry(id);
    if (!e.isValid()) {
        return false;
    }
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM media WHERE id = ?"));
    q.addBindValue(id);
    if (!q.exec()) {
        return false;
    }
    if (!e.linked) {
        QFile::remove(e.path());   // never touch a linked original
    }
    QFile::remove(thumbPath(id));
    m_entries.remove(id);
    emit changed();
    return true;
}

bool MediaLibrary::updateFromOriginal(const QString &id, QString *error)
{
    LibraryEntry e = entry(id);
    if (!e.isValid() || e.linked) {
        return true;   // links always show the current original
    }
    if (!QFileInfo::exists(e.originalPath)) {
        *error = tr("The original file %1 no longer exists.").arg(e.originalPath);
        return false;
    }
    const QString tmp = e.path() + ".new";
    QFile::remove(tmp);
    if (!QFile::copy(e.originalPath, tmp)) {
        *error = tr("%1 could not be copied into the media library.").arg(QFileInfo(e.originalPath).fileName());
        return false;
    }
    QFile::remove(e.path());
    QFile::rename(tmp, e.path());

    e.originalMtime = mtimeOf(e.originalPath);
    e.sha1 = fileSha1(e.path());
    save(e);
    prepare(e);
    emit changed();
    emit thumbnailChanged(id);
    return true;
}

bool MediaLibrary::setLinked(const QString &id, bool linked, QString *error)
{
    LibraryEntry e = entry(id);
    if (!e.isValid() || e.linked == linked) {
        return true;
    }
    if (!QFileInfo::exists(e.originalPath)) {
        *error = tr("The original file %1 no longer exists.").arg(e.originalPath);
        return false;
    }

    if (linked) {
        const QString copy = e.path();
        e.linked = true;
        e.fileName.clear();
        save(e);
        QFile::remove(copy);
    } else {
        e.fileName = e.id + "." + QFileInfo(e.originalPath).suffix().toLower();
        e.linked = false;
        if (!QFile::copy(e.originalPath, e.path())) {
            *error = tr("%1 could not be copied into the media library.").arg(QFileInfo(e.originalPath).fileName());
            return false;
        }
        e.originalMtime = mtimeOf(e.originalPath);
        e.sha1 = fileSha1(e.path());
        save(e);
    }
    prepare(e);
    emit changed();
    emit thumbnailChanged(id);
    return true;
}

// ---------------------------------------------------------------------------
// Thumbnails
// ---------------------------------------------------------------------------

QString MediaLibrary::thumbPath(const QString &id) const
{
    return filesDir() + "/" + id + ".thumb.png";
}

QImage MediaLibrary::thumbnail(const QString &id)
{
    const LibraryEntry e = entry(id);
    if (!e.isValid() || e.isMissing()) {
        return {};
    }
    // Cached thumbnail is valid as long as the file did not change (important for links)
    const QFileInfo thumb(thumbPath(id));
    if (thumb.exists() && thumb.lastModified() >= QFileInfo(e.path()).lastModified()) {
        return QImage(thumb.filePath());
    }
    const QImage image = renderThumbnail(e);
    if (!image.isNull()) {
        image.save(thumb.filePath(), "PNG");
    }
    return image;
}

QImage MediaLibrary::renderThumbnail(const LibraryEntry &e)
{
    switch (e.type) {
    case MediaItem::Image: {
        QImageReader reader(e.path());
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (size.isValid()) {
            reader.setScaledSize(size.scaled(kThumbSize, Qt::KeepAspectRatio));
        }
        return reader.read();
    }
    case MediaItem::Pdf:
        return renderPdfPage(e.path());
    case MediaItem::PowerPoint: {
        const QString pdf = PresentationConverter::cachedPdf(e.path());
        if (pdf.isEmpty()) {
            m_converter->convert(e.path());   // thumbnailChanged follows when done
            return {};
        }
        return renderPdfPage(pdf);
    }
    case MediaItem::Video:
        // Takes a moment: the thumbnail is saved and announced by thumbnailChanged when ready
        if (!m_videoThumbsPending.contains(e.id)) {
            m_videoThumbsPending.insert(e.id);
            m_videoThumbnailer->request(e.id, e.path());
        }
        return {};
    default:
        return {};
    }
}
