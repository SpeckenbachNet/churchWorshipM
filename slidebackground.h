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

#include <QColor>
#include <QImage>
#include <QJsonObject>
#include <QString>
#include <functional>

class QPainter;

// Background of the text slides (songs, bible texts, own slides).
// The event has a default, every entry can use it (Inherit) or have an own one.
// On a color or an image the text gets a soft shadow so it stays readable.
struct SlideBackground {
    enum Kind { Inherit, Black, Color, Image };

    Kind    kind = Inherit;
    QColor  color;          // Color
    QString libraryId;      // Image: entry of the media library
    QString path;           // Image: file (from the library, kept as fallback)
    int     dim = 40;       // Image: darkening in percent

    bool isInherit() const { return kind == Inherit; }
    bool hasShadow() const { return kind == Color || kind == Image; }

    // The background actually shown: 'eventDefault' if this one is Inherit
    SlideBackground resolved(const SlideBackground &eventDefault) const;

    QJsonObject toJson() const;   // empty for Inherit
    static SlideBackground fromJson(const QJsonObject &o);

    bool operator==(const SlideBackground &other) const;
    bool operator!=(const SlideBackground &other) const { return !(*this == other); }

    // Complete slide: background, 'draw' paints the text (white), with shadow if needed.
    // 'image' is the loaded background image (Image only, may be null).
    static QImage compose(const QSize &size, const SlideBackground &background, const QImage &image,
                          const std::function<void(QPainter &)> &draw);

    // Colors of secondary text (translation, credits, bible reference) on this background
    QColor mutedColor() const;
};
