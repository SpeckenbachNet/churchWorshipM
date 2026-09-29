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
#include "searchfield.h"

#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QPainter>

namespace {

// Same geometry and blend factors as the ToolbarM buttons
constexpr int   kHeight = 32;
constexpr qreal kBorder = 0.30;
constexpr qreal kHover  = 0.09;

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    return QColor::fromRgbF(a.redF()   * (1.0 - t) + b.redF()   * t,
                            a.greenF() * (1.0 - t) + b.greenF() * t,
                            a.blueF()  * (1.0 - t) + b.blueF()  * t);
}

QIcon tinted(const QString &path, const QColor &color)
{
    QPixmap pm = QIcon(path).pixmap(48, 48);
    QPainter p(&pm);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(pm.rect(), color);
    return QIcon(pm);
}

} // namespace

SearchField::SearchField(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(tr("Search..."));
    setClearButtonEnabled(true);
    setFixedHeight(kHeight);
    setAttribute(Qt::WA_MacShowFocusRect, false);   // the border shows the focus
    m_searchAction = addAction(QIcon(), QLineEdit::LeadingPosition);
    updateLook();
}

void SearchField::changeEvent(QEvent *event)
{
    QLineEdit::changeEvent(event);
    if (!m_updating && (event->type() == QEvent::ApplicationPaletteChange
                        || event->type() == QEvent::PaletteChange)) {
        updateLook();
    }
}

void SearchField::updateLook()
{
    m_updating = true;
    // Application palette: our own style sheet changes the widget palette
    const QPalette pal = QApplication::palette();
    const QColor fill   = pal.color(QPalette::Button);
    const QColor text   = pal.color(QPalette::ButtonText);
    const QColor border = mix(fill, text, kBorder);
    const QColor hover  = mix(fill, text, kHover);
    const QColor focus  = pal.color(QPalette::Highlight);

    setStyleSheet(QStringLiteral(
        "QLineEdit { background: %1; color: %2; border: 1px solid %3; border-radius: %4px;"
        " padding: 0 10px 0 4px; selection-background-color: %5; }"
        "QLineEdit:hover { background: %6; }"
        "QLineEdit:focus { border: 1px solid %5; }")
        .arg(fill.name(), text.name(), border.name())
        .arg(kHeight / 2)
        .arg(focus.name(), hover.name()));

    m_searchAction->setIcon(tinted(QStringLiteral(":icons/search"), mix(fill, text, 0.6)));
    m_updating = false;
}
