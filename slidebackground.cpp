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
#include "slidebackground.h"

#include <QPainter>

namespace {

// Soft shadow: the text shape in black, blurred by scaling it down and up again.
// Cheap and independent of the resolution (the blur is relative to the slide size).
QImage shadowOf(const QImage &layer)
{
    QImage shape = layer;
    {
        QPainter p(&shape);
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(shape.rect(), Qt::black);
    }
    const QSize small = (layer.size() / 12).expandedTo(QSize(1, 1));
    return shape.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
        .scaled(layer.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

SlideBackground SlideBackground::resolved(const SlideBackground &eventDefault) const
{
    if (!isInherit()) {
        return *this;
    }
    SlideBackground bg = eventDefault;
    if (bg.isInherit()) {
        bg.kind = Black;
    }
    return bg;
}

QJsonObject SlideBackground::toJson() const
{
    switch (kind) {
    case Inherit:
        return {};
    case Black:
        return {{"kind", "black"}};
    case Color:
        return {{"kind", "color"}, {"color", color.name()}};
    case Image:
        return {{"kind", "image"}, {"library", libraryId}, {"path", path}, {"dim", dim}};
    }
    return {};
}

SlideBackground SlideBackground::fromJson(const QJsonObject &o)
{
    SlideBackground bg;
    const QString kind = o.value("kind").toString();
    if (kind == QLatin1String("black")) {
        bg.kind = Black;
    } else if (kind == QLatin1String("color")) {
        bg.kind = Color;
        bg.color = QColor(o.value("color").toString());
    } else if (kind == QLatin1String("image")) {
        bg.kind = Image;
        bg.libraryId = o.value("library").toString();
        bg.path = o.value("path").toString();
        bg.dim = o.value("dim").toInt(40);
    }
    return bg;
}

bool SlideBackground::operator==(const SlideBackground &other) const
{
    return toJson() == other.toJson();
}

QColor SlideBackground::mutedColor() const
{
    return hasShadow() ? QColor(215, 215, 215) : QColor(150, 150, 150);
}

QImage SlideBackground::compose(const QSize &size, const SlideBackground &background, const QImage &image,
                                const std::function<void(QPainter &)> &draw)
{
    QImage slide(size, QImage::Format_RGB32);
    slide.fill(background.kind == Color && background.color.isValid() ? background.color : QColor(Qt::black));

    QPainter p(&slide);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    if (background.kind == Image && !image.isNull()) {
        // Fills the slide, the rest is cut off (no black bars)
        const QSize scaled = image.size().scaled(size, Qt::KeepAspectRatioByExpanding);
        const QRect target(QPoint((size.width() - scaled.width()) / 2, (size.height() - scaled.height()) / 2),
                           scaled);
        p.drawImage(target, image);
        p.fillRect(slide.rect(), QColor(0, 0, 0, qBound(0, background.dim, 100) * 255 / 100));
    }

    if (!background.hasShadow()) {
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setRenderHint(QPainter::Antialiasing);
        draw(p);
        return slide;
    }

    // Text on its own layer, the shadow below it
    QImage layer(size, QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    {
        QPainter lp(&layer);
        lp.setRenderHint(QPainter::TextAntialiasing);
        lp.setRenderHint(QPainter::Antialiasing);
        draw(lp);
    }
    const QImage shadow = shadowOf(layer);
    p.setOpacity(0.9);
    p.drawImage(0, 0, shadow);
    p.drawImage(0, 0, shadow);   // twice: darker close to the letters
    p.setOpacity(1.0);
    p.drawImage(0, 0, layer);
    return slide;
}
