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

#include "mediaitem.h"

#include <QDateTime>
#include <QHash>
#include <QImage>
#include <QList>
#include <QSet>
#include <QObject>
#include <QSqlDatabase>
#include <QString>

class PresentationConverter;
class VideoThumbnailer;

// One file of the media library
struct LibraryEntry {
    QString         id;              // UUID, referenced by playlist entries
    QString         title;
    MediaItem::Type type = MediaItem::Unknown;
    bool            linked = false;  // true: file stays at originalPath, nothing is copied
    QString         fileName;        // copy inside the library (only if !linked)
    QString         originalPath;    // where the file came from
    qint64          originalMtime = 0;   // modification time of the original at the last copy
    QString         sha1;
    QDateTime       added;

    bool    isValid() const { return !id.isEmpty(); }
    QString path() const;             // the file that is actually shown
    bool    isMissing() const;        // copy deleted or linked original gone (USB stick, ...)
    bool    updateAvailable() const;  // copy only: the original was changed after copying
};

// Images, PDFs and presentations that are used in the playlists.
// Files are either copied into the library (default) or only linked.
//   <AppData>/library/library.sqlite   entries
//   <AppData>/library/files/           copies and thumbnails
class MediaLibrary : public QObject {
    Q_OBJECT

public:
    explicit MediaLibrary(PresentationConverter *converter, QObject *parent = nullptr);
    ~MediaLibrary() override;

    static QString dir();
    static QString filesDir();
    static bool isSupported(const QString &path);

    QList<LibraryEntry> entries() const;          // sorted by title
    LibraryEntry        entry(const QString &id) const;

    // Returns the id of the new entry, or of the existing one if the file is already there
    QString addFile(const QString &path, bool link, QString *error);
    bool    rename(const QString &id, const QString &title);
    bool    remove(const QString &id);
    bool    updateFromOriginal(const QString &id, QString *error);
    bool    setLinked(const QString &id, bool linked, QString *error);

    // Cached preview (first page / slide); empty while a presentation is still converted
    QImage thumbnail(const QString &id);

signals:
    void changed();
    void thumbnailChanged(const QString &id);

private:
    void load();
    bool save(const LibraryEntry &e);
    void prepare(const LibraryEntry &e);   // start conversion, drop old thumbnail
    QString thumbPath(const QString &id) const;
    QImage renderThumbnail(const LibraryEntry &e);

    PresentationConverter        *m_converter;
    VideoThumbnailer             *m_videoThumbnailer = nullptr;
    QSet<QString>                 m_videoThumbsPending;   // ids whose thumbnail is being made
    QSqlDatabase                  m_db;
    QHash<QString, LibraryEntry>  m_entries;
};
