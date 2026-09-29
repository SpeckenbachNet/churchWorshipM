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

#include <QWidget>

#include "eventstore.h"

// Header above the playlist: calendar sheet with month and day, name of the event,
// weekday, time and number of entries. Painted in the look of the ToolbarM.
// A click opens the properties of the event (or the event overview if none is open).
class EventHeader : public QWidget {
    Q_OBJECT

public:
    explicit EventHeader(QWidget *parent = nullptr);

    void setEvent(const EventInfo &info);
    void setEntryCount(int count);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void paintBadge(QPainter &p, const QRectF &r, const QColor &text, const QColor &card) const;
    QString subtitle() const;

    EventInfo m_info;
    int       m_count = 0;
    bool      m_hover = false;
};
