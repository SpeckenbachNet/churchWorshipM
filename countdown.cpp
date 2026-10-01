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
#include "countdown.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

// Digits of equal width: the time does not wobble while counting
QFont digitFont(int pixelSize)
{
    QFont font = QApplication::font();
    font.setPixelSize(pixelSize);
    font.setWeight(QFont::DemiBold);
    font.setFeature(QFont::Tag("tnum"), 1);
    return font;
}

} // namespace

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

CountdownSettings CountdownSettings::defaults()
{
    CountdownSettings s;
    s.title = QApplication::translate("Countdown", "The service starts in");
    s.endText = QApplication::translate("Countdown", "Here we go!");
    return s;
}

QJsonObject CountdownSettings::toJson() const
{
    return {{"minutes", minutes}, {"title", title}, {"endText", endText}, {"corner", corner}};
}

CountdownSettings CountdownSettings::fromJson(const QJsonObject &o)
{
    CountdownSettings s = defaults();
    s.minutes = qBound(1, o.value("minutes").toInt(5), 60);
    s.title = o.value("title").toString(s.title);
    s.endText = o.value("endText").toString(s.endText);
    s.corner = o.value("corner").toBool();
    return s;
}

QString CountdownSettings::summary() const
{
    const QString time = QApplication::translate("Countdown", "%n min", "", minutes);
    return corner ? time + QStringLiteral("  ·  ") + QApplication::translate("Countdown", "small in the corner")
                  : time;
}

// ---------------------------------------------------------------------------
// Slide
// ---------------------------------------------------------------------------

QString CountdownDeck::timeText(int seconds)
{
    seconds = qMax(0, seconds);
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QImage CountdownDeck::render(int, const QSize &maxSize)
{
    const QSize size = textSlideAspect.scaled(maxSize, Qt::KeepAspectRatio);
    if (size.isEmpty()) {
        return QImage();
    }
    QImage slide = SlideBackground::compose(size, m_background, m_backgroundImage, [&](QPainter &p) {
        const QRect area = QRect(QPoint(0, 0), size).adjusted(size.width() / 16, size.height() / 12,
                                                             -size.width() / 16, -size.height() / 12);
        if (m_ending) {
            QFont font = QApplication::font();
            font.setPixelSize(size.height() / 7);
            font.setWeight(QFont::DemiBold);
            p.setFont(font);
            p.setPen(Qt::white);
            p.drawText(area, Qt::AlignCenter | Qt::TextWordWrap, m_settings.endText);
            return;
        }
        // Title above, the time large below it, both centered as one block
        QFont titleFont = QApplication::font();
        titleFont.setPixelSize(size.height() / 14);
        const QFont timeFont = digitFont(size.height() / 3);
        const int titleH = m_settings.title.isEmpty() ? 0 : QFontMetrics(titleFont).height();
        const int timeH = QFontMetrics(timeFont).height();
        const int top = area.top() + (area.height() - titleH - timeH) / 2;
        if (titleH > 0) {
            p.setFont(titleFont);
            p.setPen(m_background.mutedColor());
            p.drawText(QRect(area.left(), top, area.width(), titleH), Qt::AlignCenter, m_settings.title);
        }
        p.setFont(timeFont);
        p.setPen(Qt::white);
        p.drawText(QRect(area.left(), top + titleH, area.width(), timeH), Qt::AlignCenter, timeText(m_seconds));
    });
    if (m_ending && m_visibility < 1.0) {
        // Fades out to black
        QPainter p(&slide);
        p.fillRect(slide.rect(), QColor(0, 0, 0, qBound(0, int((1.0 - m_visibility) * 255), 255)));
    }
    return slide;
}

void CountdownDeck::paintCorner(QImage &slide, int seconds)
{
    if (slide.isNull()) {
        return;
    }
    const QFont font = digitFont(qMax(8, slide.height() / 14));
    const QFontMetrics fm(font);
    const QString text = timeText(seconds);
    const int padX = fm.height() / 2;
    const QSize box(fm.horizontalAdvance(text) + 2 * padX, fm.height() + fm.height() / 4);
    const int margin = slide.height() / 24;
    const QRectF rect(slide.width() - margin - box.width(), slide.height() - margin - box.height(),
                      box.width(), box.height());

    QPainter p(&slide);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QPainterPath path;
    path.addRoundedRect(rect, rect.height() / 2, rect.height() / 2);
    p.fillPath(path, QColor(0, 0, 0, 170));
    p.setFont(font);
    p.setPen(Qt::white);
    p.drawText(rect, Qt::AlignCenter, text);
}

// ---------------------------------------------------------------------------
// Dialog
// ---------------------------------------------------------------------------

CountdownDialog::CountdownDialog(const CountdownSettings &settings, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Countdown"));
    setStyleSheet(QStringLiteral("QLineEdit { padding: 4px 6px; }"));

    m_minutes = new QSpinBox(this);
    m_minutes->setRange(1, 60);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(settings.minutes);
    m_title = new QLineEdit(settings.title, this);
    m_endText = new QLineEdit(settings.endText, this);
    m_large = new QRadioButton(tr("Large in the middle"), this);
    m_corner = new QRadioButton(tr("Small in the corner – e.g. above the running announcements"), this);
    (settings.corner ? m_corner : m_large)->setChecked(true);

    auto *hint = new QLabel(tr("Starts as soon as the entry is chosen. At 0:00 the end text appears "
                               "and fades out to black."), this);
    hint->setWordWrap(true);
    hint->setEnabled(false);   // muted

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setVerticalSpacing(10);
    form->addRow(tr("Duration:"), m_minutes);
    form->addRow(tr("Heading:"), m_title);
    form->addRow(tr("At the end:"), m_endText);
    auto *display = new QVBoxLayout;
    display->setSpacing(4);
    display->addWidget(m_large);
    display->addWidget(m_corner);
    form->addRow(tr("Display:"), display);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(14);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addWidget(buttons);
    resize(520, sizeHint().height());
    m_minutes->setFocus();
    m_minutes->selectAll();
}

CountdownSettings CountdownDialog::settings() const
{
    CountdownSettings s;
    s.minutes = m_minutes->value();
    s.title = m_title->text().trimmed();
    s.endText = m_endText->text().trimmed();
    s.corner = m_corner->isChecked();
    return s;
}
