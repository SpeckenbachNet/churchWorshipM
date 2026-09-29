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

#include <QIcon>
#include <QString>

// Everything that describes one entry of the playlist.
// The entries themselves live as QListWidgetItems; type, source and text are stored in the roles below.
namespace MediaItem {

enum Type {
    Unknown = 0,
    Image,
    Pdf,
    PowerPoint,
    Song,       // text based: slides separated by an empty line
    Bible,      // text based
    Custom,     // text based: spontaneous own slide(s)
    YouTube,    // source = video URL, plays on the projector
    Blank       // planned empty entry: projector shows nothing
};

enum Role {
    TypeRole = Qt::UserRole + 1,  // MediaItem::Type as int
    SourceRole,                   // local file path (file based types) or URL (YouTube)
    TextRole,                     // slide text (text based types)
    BibleRole,                    // QJsonObject of a BiblePassage (bible entries chosen on the bible page)
    LibraryRole                   // id of the media library entry (file based types)
};

Type    typeFromFile(const QString &path);
bool    isTextType(Type type);
QString typeName(Type type);
QIcon   typeIcon(Type type);

// Stable identifiers for the playlist file
QString typeKey(Type type);
Type    typeFromKey(const QString &key);

// Filter strings for QFileDialog
QString mediaFileFilter();      // everything the media library accepts
QString documentFileFilter();   // PDF + presentations
QString imageFileFilter();

// Extracts the 11 character video id from any common YouTube URL (empty if not found)
QString youTubeId(const QString &url);

} // namespace MediaItem
