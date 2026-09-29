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
#include "mediaitem.h"

#include <QApplication>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QStyle>

namespace MediaItem {

Type typeFromFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "ppt" || suffix == "pptx" || suffix == "odp") {
        return PowerPoint;
    }

    const QString mime = QMimeDatabase().mimeTypeForFile(path).name();
    if (mime == QLatin1String("application/pdf")) return Pdf;
    if (mime.startsWith(QLatin1String("image/")))  return Image;
    return Unknown;
}

bool isTextType(Type type)
{
    return type == Song || type == Bible || type == Custom;
}

QString typeName(Type type)
{
    switch (type) {
    case Image:      return QApplication::translate("MediaItem", "Image");
    case Pdf:        return QApplication::translate("MediaItem", "PDF");
    case PowerPoint: return QApplication::translate("MediaItem", "Presentation");
    case Song:       return QApplication::translate("MediaItem", "Song");
    case Bible:      return QApplication::translate("MediaItem", "Bible text");
    case Custom:     return QApplication::translate("MediaItem", "Own slide");
    case YouTube:    return QApplication::translate("MediaItem", "YouTube video");
    case Blank:      return QApplication::translate("MediaItem", "Blank");
    default:         return QApplication::translate("MediaItem", "Unknown");
    }
}

QIcon typeIcon(Type type)
{
    // Theme icons first (Linux), style icons as fallback (Windows/macOS)
    const QIcon fallback = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    switch (type) {
    case Image:      return QIcon::fromTheme("image-x-generic", fallback);
    case Pdf:        return QIcon::fromTheme("application-pdf", fallback);
    case PowerPoint: return QIcon::fromTheme("x-office-presentation", fallback);
    case Song:       return QIcon::fromTheme("audio-x-generic", fallback);
    case Bible:      return QIcon::fromTheme("accessories-dictionary", fallback);
    case Custom:     return QIcon::fromTheme("text-x-generic", fallback);
    case YouTube:    return QIcon::fromTheme("video-x-generic", fallback);
    case Blank:      return QIcon::fromTheme("video-display", fallback);
    default:         return fallback;
    }
}

QString typeKey(Type type)
{
    switch (type) {
    case Image:      return "image";
    case Pdf:        return "pdf";
    case PowerPoint: return "presentation";
    case Song:       return "song";
    case Bible:      return "bible";
    case Custom:     return "custom";
    case YouTube:    return "youtube";
    case Blank:      return "blank";
    default:         return "unknown";
    }
}

Type typeFromKey(const QString &key)
{
    for (Type t : {Image, Pdf, PowerPoint, Song, Bible, Custom, YouTube, Blank}) {
        if (typeKey(t) == key) {
            return t;
        }
    }
    return Unknown;
}

QString mediaFileFilter()
{
    return QApplication::translate("MediaItem", "Images, PDF and presentations")
         + " (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg *.pdf *.ppt *.pptx *.odp);;"
         + QApplication::translate("MediaItem", "All files") + " (*)";
}

QString documentFileFilter()
{
    return QApplication::translate("MediaItem", "PDF and presentations")
         + " (*.pdf *.ppt *.pptx *.odp);;"
         + QApplication::translate("MediaItem", "All files") + " (*)";
}

QString imageFileFilter()
{
    return QApplication::translate("MediaItem", "Images")
         + " (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg);;"
         + QApplication::translate("MediaItem", "All files") + " (*)";
}

QString youTubeId(const QString &url)
{
    static const QRegularExpression re(
        QStringLiteral(R"((?:youtu\.be/|[?&]v=|/embed/|/shorts/|/live/)([A-Za-z0-9_-]{11}))"));

    const QRegularExpressionMatch match = re.match(url);
    if (match.hasMatch()) {
        return match.captured(1);
    }

    // Plain video id entered
    static const QRegularExpression idOnly(QStringLiteral(R"(^[A-Za-z0-9_-]{11}$)"));
    const QString trimmed = url.trimmed();
    return idOnly.match(trimmed).hasMatch() ? trimmed : QString();
}

} // namespace MediaItem
