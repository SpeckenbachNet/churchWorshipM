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
#include "playlistdelegate.h"
#include "mediaitem.h"

#include <QApplication>
#include <QFileInfo>
#include <QJsonObject>
#include <QPainter>

void PlaylistDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();

    // --- 1. Background / selection / focus from the style, without text and icon
    opt.text.clear();
    opt.icon = QIcon();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    painter->save();

    const auto type     = MediaItem::Type(index.data(MediaItem::TypeRole).toInt());
    const QString title = index.data(Qt::DisplayRole).toString();
    const bool selected = opt.state & QStyle::State_Selected;

    const QRect r = opt.rect.adjusted(padding, padding, -padding, -padding);

    // --- 2. Icon
    const QRect iconRect(r.left(), r.center().y() - iconSize / 2, iconSize, iconSize);
    MediaItem::typeIcon(type).paint(painter, iconRect, Qt::AlignCenter,
                                    selected ? QIcon::Selected : QIcon::Normal);

    // --- 3. Colors
    QPalette::ColorGroup cg = (opt.state & QStyle::State_Enabled) ? QPalette::Normal
                                                                  : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active)) {
        cg = QPalette::Inactive;
    }
    const QColor textColor = opt.palette.color(cg, selected ? QPalette::HighlightedText
                                                            : QPalette::Text);
    QColor subColor = textColor;
    subColor.setAlphaF(0.65);

    // --- 4. Text lines
    QFont titleFont = opt.font;
    titleFont.setBold(true);
    const QFontMetrics fmTitle(titleFont);
    const QFontMetrics fmSub(opt.font);

    const int textX  = iconRect.right() + padding + 4;
    const int textW  = r.right() - textX;
    const int blockH = fmTitle.height() + lineSpacing + fmSub.height();
    const int top    = r.center().y() - blockH / 2;

    // Second line: file name for files, first line of text for text slides
    QString detail;
    const QJsonObject bible = index.data(MediaItem::BibleRole).toJsonObject();
    if (!bible.isEmpty()) {
        detail = bible.value("abbreviation").toString();
    } else if (MediaItem::isTextType(type)) {
        detail = index.data(MediaItem::TextRole).toString().section('\n', 0, 0).trimmed();
    } else if (type == MediaItem::YouTube) {
        detail = index.data(MediaItem::SourceRole).toString();
    } else {
        detail = QFileInfo(index.data(MediaItem::SourceRole).toString()).fileName();
    }
    QString sub = detail.isEmpty() ? MediaItem::typeName(type)
                                   : MediaItem::typeName(type) + QStringLiteral("  ·  ") + detail;

    // File based entries whose file is gone (USB stick, deleted, ...) are marked before the service
    const bool fileBased = type == MediaItem::Image || type == MediaItem::Pdf || type == MediaItem::PowerPoint;
    if (fileBased && !QFileInfo::exists(index.data(MediaItem::SourceRole).toString())) {
        sub = tr("File missing") + QStringLiteral("  ·  ") + sub;
        subColor = QColor(220, 80, 70);
    }

    painter->setFont(titleFont);
    painter->setPen(textColor);
    painter->drawText(QRect(textX, top, textW, fmTitle.height()), Qt::AlignLeft | Qt::AlignVCenter,
                      fmTitle.elidedText(title, Qt::ElideRight, textW));

    painter->setFont(opt.font);
    painter->setPen(subColor);
    painter->drawText(QRect(textX, top + fmTitle.height() + lineSpacing, textW, fmSub.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      fmSub.elidedText(sub, Qt::ElideRight, textW));

    painter->restore();
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFont titleFont = option.font;
    titleFont.setBold(true);

    const int textH = QFontMetrics(titleFont).height() + lineSpacing
                    + QFontMetrics(option.font).height();
    const int h = qMax(textH, iconSize) + 2 * padding;

    return QSize(QStyledItemDelegate::sizeHint(option, index).width(), h);
}
