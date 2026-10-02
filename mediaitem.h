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
#include <QUrl>

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
    Blank,      // planned empty entry: projector shows nothing
    Countdown,  // time running down before the service (CountdownRole)
    Video       // local video file (VideoRole: what happens at the end)
};

enum Role {
    TypeRole = Qt::UserRole + 1,  // MediaItem::Type as int
    SourceRole,                   // local file path (file based types) or URL (YouTube)
    TextRole,                     // slide text (text based types)
    BibleRole,                    // QJsonObject of a BiblePassage (bible entries chosen on the bible page)
    LibraryRole,                  // id of the media library entry (file based types)
    SongRole,                     // QJsonObject {id, order} of a song from the song library
    BackgroundRole,               // QJsonObject of the own SlideBackground (text types), empty = as event
    AutoAdvanceRole,              // seconds between the slides of a loop (announcements), 0 = off
    CountdownRole,                // QJsonObject of the CountdownSettings
    VideoRole,                    // QJsonObject {"end": "black" | "last" | "loop"}
    YouTubeStatusRole             // YouTubeStatus as int, checked online (not saved)
};

// Result of asking YouTube about a video (oEmbed)
enum YouTubeStatus {
    YouTubeUnchecked = 0,   // not checked (yet), or no internet
    YouTubeOk,
    YouTubeNotFound,        // wrong link: no such video
    YouTubeNotEmbeddable    // private, or the owner allows no playback in other programs
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

// Common video formats (the decoders come with Qt Multimedia)
bool isVideoSuffix(const QString &suffix);

// Extracts the 11 character video id from any common YouTube URL (empty if not found)
QString youTubeId(const QString &url);
// Asking YouTube about a video (oEmbed, no API key needed)
QUrl          youTubeCheckUrl(const QString &url);
YouTubeStatus youTubeStatusFromHttp(int httpCode);   // 200 ok, 400/404 not found, 401/403 not embeddable

} // namespace MediaItem
