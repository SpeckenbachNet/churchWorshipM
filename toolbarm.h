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
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QToolButton>

class ToolbarM : public QFrame {
    Q_OBJECT

public:
    // Blend factors towards the text color (0.0 = base color, 1.0 = text color).
    // Central place to tune contrast of toolbar and buttons.
    struct ShadeFactors {
        qreal toolbarBackground = 0.00;  // toolbar fill
        qreal toolbarDarkEdge   = 0.15;  // top/left edge
        qreal toolbarLightEdge  = 0.05;  // bottom/right edge
        qreal buttonHover       = 0.09;
        qreal buttonPressed     = 0.18;
        qreal buttonBorder      = 0.30;
        qreal buttonBorderOff   = 0.18;  // border of disabled buttons
        qreal separator         = 0.40;
    };

    explicit ToolbarM(QWidget *parent = nullptr);

    void setShadeFactors(const ShadeFactors &factors);
    ShadeFactors shadeFactors() const { return shade; }

    // One button function for all variants (text only, icon only, text + icon).
    // NOTE: Parameter order matches the implementation (useSeparator before iconRight).
    QToolButton* addButton(const QString &objName,
                           const QString &text,
                           const QString &iconPath,
                           bool useSeparator = false,
                           bool iconRight = false);
    void addSpacer();

protected:
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateColors();
    void paintButton(QToolButton *btn);
    QPixmap getTintedPixmap(const QIcon &icon, const QSize &size, const QColor &color) const;

    // Blends a towards b (t = 0.0 -> a, t = 1.0 -> b).
    // Used to derive borders/hover colors with guaranteed contrast in light AND dark themes.
    static QColor mix(const QColor &a, const QColor &b, qreal t);

    QHBoxLayout *mainLayout = nullptr;

    // Palette colors (refreshed on palette/style change)
    QColor windowColor;
    QColor textColor;
    QColor buttonColor;
    QColor buttonTextColor;
    QColor disabledColor;
    QColor disabledTextColor;

    ShadeFactors shade;

    // Geometry
    int   buttonHeight      = 32;
    QSize iconSize{22, 22};
    int   iconSpacing       = 6;   // gap between icon and text
    int   separatorSpace    = 14;  // room for the separator line
    int   horizontalPadding = 14;  // left/right padding inside a button
    qreal buttonRadius      = 15.0;
    qreal toolbarRadius     = 18.0;
};