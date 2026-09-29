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

#include "biblepassage.h"
#include "mediaitem.h"

#include <QImage>
#include <QSize>
#include <QString>
#include <memory>

// The slides of one playlist entry.
// Slides are rendered on demand in the requested size, so the same deck
// serves thumbnails, the control preview and the full resolution projector output.
class SlideDeck {
public:
    virtual ~SlideDeck() = default;

    virtual int count() const = 0;

    // Renders slide 'index' as large as possible inside 'maxSize' (aspect ratio is kept)
    virtual QImage render(int index, const QSize &maxSize) = 0;

    // Returns nullptr and fills 'error' if the entry cannot be loaded
    // 'bible' is only used for bible entries selected from an installed bible (BiblePassage JSON)
    // 'credits' (text entries): small lines at the bottom of the first slide, e.g. the
    // copyright and CCLI numbers of a song
    static std::unique_ptr<SlideDeck> create(MediaItem::Type type, const QString &source,
                                             const QString &text, const QJsonObject &bible,
                                             QString *error, const QString &credits = {});

    // Bible passage directly (used for the preview on the bible page)
    static std::unique_ptr<SlideDeck> createBible(const BiblePassage &passage);

    // Aspect ratio used for text slides
    static constexpr QSize textSlideAspect{16, 9};
};
