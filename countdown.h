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

#include "slidedeck.h"

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLineEdit;
class QRadioButton;
class QSpinBox;

// Countdown before the service, e.g. "The service starts in 4:59".
// Large in the middle, or small in a corner above whatever is shown (announcements).
// At 0:00 the end text ("Here we go!") is shown and fades out to black.
struct CountdownSettings {
    int     minutes = 5;
    QString title;      // above the time
    QString endText;    // shown at 0:00
    bool    corner = false;

    static CountdownSettings defaults();
    QJsonObject toJson() const;
    static CountdownSettings fromJson(const QJsonObject &o);
    QString summary() const;   // "5 min", "5 min · corner"
};

// The one slide of a countdown; what it shows is set from outside every second
class CountdownDeck : public SlideDeck {
public:
    explicit CountdownDeck(const CountdownSettings &settings) : m_settings(settings) {}

    void setSecondsLeft(int seconds) { m_seconds = seconds; m_ending = false; }
    void setEnding(qreal visibility) { m_ending = true; m_visibility = visibility; }   // end text, 1 -> 0

    int count() const override { return 1; }
    QImage render(int index, const QSize &maxSize) override;

    static QString timeText(int seconds);   // "4:59"
    // Small time in the bottom right corner of 'slide' (corner mode)
    static void paintCorner(QImage &slide, int seconds);

private:
    CountdownSettings m_settings;
    int   m_seconds = 0;
    bool  m_ending = false;
    qreal m_visibility = 1.0;
};

// Creates / edits a countdown entry
class CountdownDialog : public QDialog {
    Q_OBJECT

public:
    explicit CountdownDialog(const CountdownSettings &settings, QWidget *parent = nullptr);
    CountdownSettings settings() const;

private:
    QSpinBox     *m_minutes = nullptr;
    QLineEdit    *m_title = nullptr;
    QLineEdit    *m_endText = nullptr;
    QRadioButton *m_large = nullptr;
    QRadioButton *m_corner = nullptr;
};
