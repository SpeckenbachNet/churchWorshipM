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

#include "eventstore.h"

#include <QDialog>

class QComboBox;
class QDateEdit;
class QLineEdit;
class QPlainTextEdit;

// Create an event / a template, or edit its name, date, time and note.
// New events can start with the playlist of a template.
class EventDialog : public QDialog {
    Q_OBJECT

public:
    // 'info' prefills the fields; an info without id means "new".
    // 'templates' are offered for new events only.
    EventDialog(const EventInfo &info, const QList<EventInfo> &templates, QWidget *parent = nullptr);

    EventInfo info() const;
    QString   templateId() const;   // empty = start with an empty playlist

    static QDate nextSunday();

private:
    QTime time() const;   // invalid while the typed time is incomplete

    EventInfo       m_info;
    QString         m_suggestedName;   // name taken over from a template, replaced when the template changes

    QLineEdit      *m_nameEdit = nullptr;
    QDateEdit      *m_dateEdit = nullptr;
    QComboBox      *m_timeCombo = nullptr;
    QPlainTextEdit *m_noteEdit = nullptr;
    QComboBox      *m_templateCombo = nullptr;
};
