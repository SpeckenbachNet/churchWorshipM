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
#include "toolbarm.h"

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariant>

ToolbarM::ToolbarM(QWidget *parent)
    : QFrame(parent)
{
    mainLayout = qobject_cast<QHBoxLayout*>(layout());
    if (!mainLayout) {
        mainLayout = new QHBoxLayout(this);
    }

    mainLayout->setContentsMargins(10, 7, 10, 7);
    mainLayout->setSpacing(8);
    mainLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    updateColors();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ToolbarM::addSpacer()
{
    mainLayout->addStretch(1);
}

QToolButton* ToolbarM::addButton(const QString &objName, const QString &text,
                                 const QString &iconPath, bool useSeparator, bool iconRight)
{
    QToolButton *btn = new QToolButton(this);
    btn->setObjectName(objName);
    btn->setAttribute(Qt::WA_NoSystemBackground); // we paint everything ourselves
    btn->setIconSize(iconSize);
    btn->setFixedHeight(buttonHeight);

    if (!text.isEmpty()) {
        btn->setText(text);
    }

    // Load the icon once and keep it on the button (instead of loading it on every paint)
    const QIcon icon = iconPath.isEmpty() ? QIcon() : QIcon(iconPath);
    const bool hasIcon = !icon.isNull();
    const bool hasText = !text.isEmpty();

    btn->setProperty("tbIcon", QVariant::fromValue(icon));
    btn->setProperty("iconPath", iconPath);
    btn->setProperty("useSeparator", useSeparator);
    btn->setProperty("iconRight", iconRight);

    // Width calculation uses the SAME values as paintButton(), so padding stays even
    if (hasIcon && !hasText) {
        // Icon only
        btn->setFixedSize(qRound(buttonHeight * 1.8), buttonHeight);
    } else {
        const int textW = btn->fontMetrics().horizontalAdvance(text);
        int contentW = textW;
        if (hasIcon) {
            contentW += iconSize.width() + iconSpacing;
            if (useSeparator) {
                contentW += separatorSpace;
            }
        }
        btn->setFixedWidth(contentW + 2 * horizontalPadding);
    }

    btn->installEventFilter(this);
    mainLayout->addWidget(btn);

    return btn;
}

// ---------------------------------------------------------------------------
// Colors
// ---------------------------------------------------------------------------

QColor ToolbarM::mix(const QColor &a, const QColor &b, qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF()   * (1.0 - t) + b.redF()   * t,
                            a.greenF() * (1.0 - t) + b.greenF() * t,
                            a.blueF()  * (1.0 - t) + b.blueF()  * t,
                            a.alphaF());
}

void ToolbarM::updateColors()
{
    const QPalette pal = palette();

    windowColor       = pal.color(QPalette::Window);
    textColor         = pal.color(QPalette::WindowText);
    buttonColor       = pal.color(QPalette::Button);
    buttonTextColor   = pal.color(QPalette::ButtonText);
    disabledColor     = pal.color(QPalette::Disabled, QPalette::Button);
    disabledTextColor = pal.color(QPalette::Disabled, QPalette::ButtonText);
}

void ToolbarM::setShadeFactors(const ShadeFactors &factors)
{
    shade = factors;
    update();
    for (QToolButton *btn : findChildren<QToolButton*>()) {
        btn->update();
    }
}

void ToolbarM::changeEvent(QEvent *event)
{
    switch (event->type()) {
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
    case QEvent::StyleChange:
        updateColors();
        update();
        // Buttons use our cached colors, so repaint them as well
        for (QToolButton *btn : findChildren<QToolButton*>()) {
            btn->update();
        }
        break;
    default:
        break;
    }

    QFrame::changeEvent(event); // important: never swallow the base implementation
}

// ---------------------------------------------------------------------------
// Button painting
// ---------------------------------------------------------------------------

bool ToolbarM::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        if (QToolButton *btn = qobject_cast<QToolButton*>(obj)) {
            paintButton(btn);
            return true;
        }
    }
    return QFrame::eventFilter(obj, event);
}

void ToolbarM::paintButton(QToolButton *btn)
{
    QPainter p(btn);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QIcon icon        = btn->property("tbIcon").value<QIcon>();
    const bool hasText      = !btn->text().isEmpty();
    const bool hasIcon      = !icon.isNull();
    const bool useSeparator = btn->property("useSeparator").toBool();
    const bool iconRight    = btn->property("iconRight").toBool();
    const bool isEnabled    = btn->isEnabled();
    const bool isPressed    = btn->isDown();
    const bool isHovered    = btn->underMouse();

    // --- 1. Colors: everything derived from button + text color -> visible in every theme
    QColor fill = buttonColor;
    QColor fg   = buttonTextColor;

    if (!isEnabled) {
        fill = disabledColor;
        fg   = disabledTextColor;
    } else if (isPressed) {
        fill = mix(buttonColor, buttonTextColor, shade.buttonPressed);
    } else if (isHovered) {
        fill = mix(buttonColor, buttonTextColor, shade.buttonHover);
    }

    const QColor border    = mix(fill, fg, isEnabled ? shade.buttonBorder : shade.buttonBorderOff);
    const QColor separator = mix(fill, fg, shade.separator);

    // --- 2. Button body
    const QRectF r = QRectF(btn->rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = qMin(buttonRadius, r.height() / 2.0);

    p.setPen(QPen(border, 1));
    p.setBrush(fill);
    p.drawRoundedRect(r, radius, radius);

    p.setFont(btn->font());

    const int iconW = iconSize.width();
    const int iconH = iconSize.height();

    // --- 3. Content
    if (hasIcon && hasText) {
        // TEXT AND ICON
        const QPixmap iconPixmap = getTintedPixmap(icon, iconSize, fg);
        const int textW    = p.fontMetrics().horizontalAdvance(btn->text());
        const int sepSpace = useSeparator ? separatorSpace : 0;
        const int totalW   = iconW + iconSpacing + textW + sepSpace;

        const int startX = (btn->width() - totalW) / 2;
        const int iconY  = (btn->height() - iconH) / 2;

        int iconX, textX, sepX;
        if (iconRight) {
            textX = startX;
            sepX  = startX + textW + iconSpacing / 2 + sepSpace / 2;
            iconX = startX + textW + iconSpacing + sepSpace;
        } else {
            iconX = startX;
            sepX  = startX + iconW + iconSpacing / 2 + sepSpace / 2;
            textX = startX + iconW + iconSpacing + sepSpace;
        }

        p.drawPixmap(QRect(iconX, iconY, iconW, iconH), iconPixmap);

        p.setPen(fg);
        p.drawText(QRect(textX, 0, textW, btn->height()),
                   Qt::AlignVCenter | Qt::AlignLeft, btn->text());

        if (useSeparator) {
            p.setPen(QPen(separator, 1));
            // +0.5 keeps the 1px line crisp with antialiasing
            p.drawLine(QPointF(sepX + 0.5, 7), QPointF(sepX + 0.5, btn->height() - 7));
        }
    } else if (hasIcon) {
        // ONLY ICON
        const QPixmap iconPixmap = getTintedPixmap(icon, iconSize, fg);
        const int iconX = (btn->width() - iconW) / 2;
        const int iconY = (btn->height() - iconH) / 2;
        p.drawPixmap(QRect(iconX, iconY, iconW, iconH), iconPixmap);
    } else if (hasText) {
        // ONLY TEXT
        p.setPen(fg);
        p.drawText(btn->rect(), Qt::AlignCenter, btn->text());
    }
}

QPixmap ToolbarM::getTintedPixmap(const QIcon &icon, const QSize &size, const QColor &color) const
{
    if (icon.isNull()) {
        return QPixmap();
    }

    // Qt 6: request the pixmap directly for the screen's device pixel ratio
    QPixmap pixmap = icon.pixmap(size, devicePixelRatioF());
    if (pixmap.isNull()) {
        return pixmap;
    }

    QPainter pixPainter(&pixmap);
    pixPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    pixPainter.fillRect(pixmap.rect(), color);
    pixPainter.end();

    return pixmap;
}

// ---------------------------------------------------------------------------
// Toolbar painting
// ---------------------------------------------------------------------------

void ToolbarM::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius   = qMin(toolbarRadius, r.height() / 2.0);
    const qreal diameter = radius * 2.0;

    // Colors derived from Window + WindowText -> contrast in light AND dark themes
    const QColor bgColor   = mix(windowColor, textColor, shade.toolbarBackground);
    const QColor darkEdge  = mix(bgColor, textColor, shade.toolbarDarkEdge);
    const QColor lightEdge = mix(bgColor, textColor, shade.toolbarLightEdge);

    // 1. Background
    QPainterPath bg;
    bg.addRoundedRect(r, radius, radius);
    p.fillPath(bg, bgColor);

    // 2. Dark edge (top + left + part of the top-right and bottom-left corners)
    // Qt angles: 0 = right, 90 = top, 180 = left, 270 = bottom
    QPainterPath darkPath;
    darkPath.arcMoveTo(r.right() - diameter, r.top(), diameter, diameter, 45);
    darkPath.arcTo(r.right() - diameter, r.top(), diameter, diameter, 45, 45);
    darkPath.lineTo(r.left() + radius, r.top());
    darkPath.arcTo(r.left(), r.top(), diameter, diameter, 90, 90);
    darkPath.lineTo(r.left(), r.bottom() - radius);
    darkPath.arcTo(r.left(), r.bottom() - diameter, diameter, diameter, 180, 45);

    p.setPen(QPen(darkEdge, 1));
    p.drawPath(darkPath);

    // 3. Light edge (bottom + right + rest of the corners)
    QPainterPath lightPath;
    lightPath.arcMoveTo(r.left(), r.bottom() - diameter, diameter, diameter, 225);
    lightPath.arcTo(r.left(), r.bottom() - diameter, diameter, diameter, 225, 45);
    lightPath.lineTo(r.right() - radius, r.bottom());
    lightPath.arcTo(r.right() - diameter, r.bottom() - diameter, diameter, diameter, 270, 90);
    lightPath.lineTo(r.right(), r.top() + radius);
    lightPath.arcTo(r.right() - diameter, r.top(), diameter, diameter, 0, 45);

    p.setPen(QPen(lightEdge, 1));
    p.drawPath(lightPath);
}