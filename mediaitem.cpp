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
#include <QHash>
#include <QMimeDatabase>
#include <QPainter>
#include <QRegularExpression>
#include <QStyle>
#include <QUrl>
#include <QUrlQuery>

namespace {

// Our SVG icons are black; recolor them with the text colors so they stay visible in dark mode
QIcon tintedIcon(const QString &path)
{
    const QPixmap source = QIcon(path).pixmap(64, 64);
    auto tinted = [&source](const QColor &color) {
        QPixmap pm = source;
        QPainter p(&pm);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(pm.rect(), color);
        return pm;
    };
    const QPalette palette = QApplication::palette();
    QIcon icon;
    icon.addPixmap(tinted(palette.color(QPalette::Text)), QIcon::Normal);
    icon.addPixmap(tinted(palette.color(QPalette::HighlightedText)), QIcon::Selected);
    return icon;
}

} // namespace

namespace MediaItem {

Type typeFromFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "ppt" || suffix == "pptx" || suffix == "odp") {
        return PowerPoint;
    }

    if (isVideoSuffix(suffix)) {
        return Video;
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
    case Countdown:  return QApplication::translate("MediaItem", "Countdown");
    case Video:      return QApplication::translate("MediaItem", "Video");
    default:         return QApplication::translate("MediaItem", "Unknown");
    }
}

QIcon typeIcon(Type type)
{
    // Own symbols on every platform (theme icons exist only on Linux). Tinted once per
    // palette and cached: the playlist asks for the icon on every paint.
    static QHash<int, QIcon> cache;
    static QRgb cachedColor = 0;
    const QRgb color = QApplication::palette().color(QPalette::Text).rgba();
    if (color != cachedColor) {
        cache.clear();   // light <-> dark mode switched
        cachedColor = color;
    }
    auto it = cache.constFind(int(type));
    if (it != cache.constEnd()) {
        return *it;
    }

    QString path;
    switch (type) {
    case Image:      path = QStringLiteral(":icons/type_image"); break;
    case Pdf:        path = QStringLiteral(":icons/type_pdf"); break;
    case PowerPoint: path = QStringLiteral(":icons/type_presentation"); break;
    case Song:       path = QStringLiteral(":icons/type_song"); break;
    case Bible:      path = QStringLiteral(":icons/type_bible"); break;
    case Custom:     path = QStringLiteral(":icons/type_custom"); break;
    case YouTube:    path = QStringLiteral(":icons/type_youtube"); break;
    case Blank:      path = QStringLiteral(":icons/black_screen"); break;   // same symbol as "Black"
    case Countdown:  path = QStringLiteral(":icons/type_countdown"); break;
    case Video:      path = QStringLiteral(":icons/type_video"); break;
    default:         return QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    }
    return cache.insert(int(type), tintedIcon(path)).value();
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
    case Countdown:  return "countdown";
    case Video:      return "video";
    default:         return "unknown";
    }
}

Type typeFromKey(const QString &key)
{
    for (Type t : {Image, Pdf, PowerPoint, Song, Bible, Custom, YouTube, Blank, Countdown, Video}) {
        if (typeKey(t) == key) {
            return t;
        }
    }
    return Unknown;
}

QString mediaFileFilter()
{
    return QApplication::translate("MediaItem", "Images, videos, PDF and presentations")
         + " (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg *.pdf *.ppt *.pptx *.odp"
           " *.mp4 *.m4v *.mov *.mkv *.webm *.avi *.wmv *.mpg *.mpeg);;"
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

bool isVideoSuffix(const QString &suffix)
{
    static const QStringList suffixes{"mp4", "m4v", "mov", "mkv", "webm", "avi", "wmv", "mpg", "mpeg"};
    return suffixes.contains(suffix.toLower());
}

QUrl youTubeCheckUrl(const QString &url)
{
    QUrl request(QStringLiteral("https://www.youtube.com/oembed"));
    request.setQuery(QUrlQuery{{"url", url}, {"format", "json"}});
    return request;
}

YouTubeStatus youTubeStatusFromHttp(int httpCode)
{
    switch (httpCode) {
    case 200:
        return YouTubeOk;
    case 400:
    case 404:
        return YouTubeNotFound;
    case 401:
    case 403:
        return YouTubeNotEmbeddable;
    default:
        return YouTubeUnchecked;
    }
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
