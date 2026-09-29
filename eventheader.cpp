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
#include "eventheader.h"

#include <QEnterEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

// Same radius and blend factors as the ToolbarM
constexpr qreal kRadius   = 18.0;
constexpr int   kHeight   = 60;
constexpr int   kPadding  = 10;
constexpr int   kBadge    = kHeight - 2 * kPadding;
constexpr qreal kBorder   = 0.20;
constexpr qreal kHover    = 0.06;
constexpr qreal kMuted    = 0.55;

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF()   * (1.0 - t) + b.redF()   * t,
                            a.greenF() * (1.0 - t) + b.greenF() * t,
                            a.blueF()  * (1.0 - t) + b.blueF()  * t);
}

} // namespace

EventHeader::EventHeader(QWidget *parent)
    : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setEvent({});
}

void EventHeader::setEvent(const EventInfo &info)
{
    m_info = info;
    setToolTip(!m_info.isValid()   ? tr("Open the event overview")
               : m_info.note.isEmpty() ? tr("Edit the properties of the event")
                                       : m_info.note);
    update();
}

void EventHeader::setEntryCount(int count)
{
    if (m_count != count) {
        m_count = count;
        update();
    }
}

QSize EventHeader::sizeHint() const
{
    return {300, kHeight};
}

QSize EventHeader::minimumSizeHint() const
{
    return {kBadge + 3 * kPadding + 60, kHeight};
}

QString EventHeader::subtitle() const
{
    const QString entries = tr("%n entries", nullptr, m_count);
    if (!m_info.isValid()) {
        return tr("Create a new event or open one");
    }
    if (m_info.isTemplate) {
        return tr("Template") + QStringLiteral(" · ") + entries;
    }
    const QLocale locale;
    QStringList parts{locale.dayName(m_info.date.dayOfWeek())};
    if (m_info.time.isValid()) {
        parts << tr("%1 h").arg(locale.toString(m_info.time, QStringLiteral("HH:mm")));
    }
    parts << entries;
    return parts.join(QStringLiteral(" · "));
}

void EventHeader::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QPalette &pal = palette();
    const QColor window = pal.color(QPalette::Window);
    const QColor text   = pal.color(QPalette::WindowText);
    const QColor card   = m_hover ? mix(window, text, kHover) : window;

    // Card
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath bg;
    bg.addRoundedRect(r, kRadius, kRadius);
    p.fillPath(bg, card);
    p.setPen(QPen(mix(window, text, kBorder), 1));
    p.drawPath(bg);

    // Calendar sheet
    const QRectF badge(kPadding, (height() - kBadge) / 2.0, kBadge, kBadge);
    paintBadge(p, badge, text, card);

    // Name and subtitle
    const qreal x = badge.right() + kPadding;
    const QRectF textRect(x, badge.top() + 2, width() - x - kPadding - 4, badge.height() - 4);

    QFont nameFont = font();
    nameFont.setPointSizeF(nameFont.pointSizeF() * 1.25);
    nameFont.setBold(true);
    const QFontMetrics nameFm(nameFont);
    const QString name = m_info.isValid() ? m_info.name : tr("No event open");

    QFont subFont = font();
    const QFontMetrics subFm(subFont);

    const qreal total = nameFm.height() + 2 + subFm.height();
    const qreal top = textRect.top() + (textRect.height() - total) / 2.0;

    p.setFont(nameFont);
    p.setPen(m_info.isValid() ? text : mix(card, text, kMuted));
    p.drawText(QRectF(textRect.left(), top, textRect.width(), nameFm.height()),
               Qt::AlignLeft | Qt::AlignVCenter,
               nameFm.elidedText(name, Qt::ElideRight, int(textRect.width())));

    p.setFont(subFont);
    p.setPen(mix(card, text, kMuted));
    p.drawText(QRectF(textRect.left(), top + nameFm.height() + 2, textRect.width(), subFm.height()),
               Qt::AlignLeft | Qt::AlignVCenter,
               subFm.elidedText(subtitle(), Qt::ElideRight, int(textRect.width())));
}

void EventHeader::paintBadge(QPainter &p, const QRectF &r, const QColor &text, const QColor &card) const
{
    const qreal radius = 8.0;
    const qreal stripe = r.height() * 0.32;
    const QColor accent = palette().color(QPalette::Active, QPalette::Highlight);
    const bool dated = m_info.isValid() && !m_info.isTemplate;

    QPainterPath sheet;
    sheet.addRoundedRect(r, radius, radius);
    p.fillPath(sheet, mix(card, text, 0.10));

    // Colored stripe on top, like a tear-off calendar
    QPainterPath top;
    top.addRect(QRectF(r.left(), r.top(), r.width(), stripe));
    p.fillPath(sheet.intersected(top), dated ? accent : mix(card, text, 0.30));

    const QRectF stripeRect(r.left(), r.top(), r.width(), stripe);
    const QRectF bodyRect(r.left(), r.top() + stripe, r.width(), r.height() - stripe);

    QFont small = font();
    small.setPointSizeF(small.pointSizeF() * 0.7);
    small.setBold(true);
    QFont big = font();
    big.setBold(true);

    const QLocale locale;
    QString head;
    QString body;
    if (dated) {
        head = locale.toString(m_info.date, QStringLiteral("MMM")).remove('.').toUpper();
        body = QString::number(m_info.date.day());
        big.setPointSizeF(big.pointSizeF() * 1.35);
    } else if (m_info.isValid()) {
        head = tr("TMPL", "short label on the calendar sheet of a template");
        body = QStringLiteral("…");
        big.setPointSizeF(big.pointSizeF() * 1.35);
    } else {
        body = QStringLiteral("+");
        big.setPointSizeF(big.pointSizeF() * 1.5);
    }

    p.setFont(small);
    p.setPen(dated ? palette().color(QPalette::Active, QPalette::HighlightedText) : text);
    p.drawText(stripeRect, Qt::AlignCenter, head);

    p.setFont(big);
    p.setPen(text);
    p.drawText(bodyRect, Qt::AlignCenter, body);
}

void EventHeader::enterEvent(QEnterEvent *event)
{
    m_hover = true;
    update();
    QWidget::enterEvent(event);
}

void EventHeader::leaveEvent(QEvent *event)
{
    m_hover = false;
    update();
    QWidget::leaveEvent(event);
}

void EventHeader::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
        emit clicked();
    }
    QWidget::mouseReleaseEvent(event);
}
