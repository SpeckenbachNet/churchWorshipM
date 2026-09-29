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
#include "eventdialog.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {
const QString kTimeFormat  = QStringLiteral("HH:mm");
const QString kTimeParsing = QStringLiteral("H:mm");   // also accepts "9:30" when typed
} // namespace

EventDialog::EventDialog(const EventInfo &info, const QList<EventInfo> &templates, QWidget *parent)
    : QDialog(parent),
    m_info(info)
{
    const bool isNew = !info.isValid();
    if (info.isTemplate) {
        setWindowTitle(isNew ? tr("New template") : tr("Template properties"));
    } else {
        setWindowTitle(isNew ? tr("New event") : tr("Event properties"));
    }
    resize(520, 420);

    // Text fields: some room between frame and text (the native style puts the text at the edge)
    setStyleSheet(QStringLiteral("QLineEdit, QAbstractSpinBox { padding: 4px 6px; }"));

    m_nameEdit = new QLineEdit(info.name, this);

    m_dateEdit = new QDateEdit(info.date.isValid() ? info.date : nextSunday(), this);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDisplayFormat(QLocale().dateFormat(QLocale::LongFormat));

    // Time as editable combo box: same look and height as the date field (a QTimeEdit is a
    // tiny spin box on macOS). Quarter hours to choose, any other time can be typed.
    m_timeCombo = new QComboBox(this);
    m_timeCombo->setEditable(true);
    m_timeCombo->setInsertPolicy(QComboBox::NoInsert);
    m_timeCombo->setMaxVisibleItems(12);
    m_timeCombo->setMinimumWidth(110);
    for (QTime t(0, 0); ; t = t.addSecs(15 * 60)) {
        m_timeCombo->addItem(t.toString(kTimeFormat));
        if (t == QTime(23, 45)) {
            break;
        }
    }
    m_timeCombo->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral(R"(([01]?\d|2[0-3]):[0-5]\d)")), m_timeCombo));
    m_timeCombo->setCurrentText((info.time.isValid() ? info.time : QTime(10, 0)).toString(kTimeFormat));

    m_noteEdit = new QPlainTextEdit(info.note, this);
    m_noteEdit->document()->setDocumentMargin(8);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // OK only makes sense with a name and a complete time
    auto updateOk = [this, buttons]() {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_nameEdit->text().trimmed().isEmpty()
                                                          && (m_info.isTemplate || time().isValid()));
    };
    connect(m_nameEdit, &QLineEdit::textChanged, this, updateOk);
    connect(m_timeCombo, &QComboBox::currentTextChanged, this, updateOk);
    updateOk();

    // All fields as wide as the dialog (the macOS default keeps them at their minimum width)
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setVerticalSpacing(12);
    form->setHorizontalSpacing(12);
    form->addRow(tr("Name:"), m_nameEdit);
    if (!info.isTemplate) {
        // Date and time in one row: the time needs little room
        auto *when = new QHBoxLayout;
        when->setSpacing(12);
        when->addWidget(m_dateEdit, 1);
        when->addWidget(new QLabel(tr("Time:"), this));
        when->addWidget(m_timeCombo);
        form->addRow(tr("Date:"), when);
    } else {
        m_dateEdit->hide();
        m_timeCombo->hide();
    }

    if (isNew && !info.isTemplate && !templates.isEmpty()) {
        m_templateCombo = new QComboBox(this);
        m_templateCombo->addItem(tr("(empty playlist)"));
        for (const EventInfo &t : templates) {
            m_templateCombo->addItem(t.name, t.id);
        }
        form->addRow(tr("Template:"), m_templateCombo);

        // The template's name is a good default as long as the user has not typed an own one
        connect(m_templateCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
            const QString name = index > 0 ? m_templateCombo->itemText(index) : QString();
            if (m_nameEdit->text().trimmed().isEmpty() || m_nameEdit->text() == m_suggestedName) {
                m_nameEdit->setText(name);
            }
            m_suggestedName = name;
        });
        m_templateCombo->setCurrentIndex(1);   // with templates, the first one is the usual start
    }
    form->addRow(tr("Note:"), m_noteEdit);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(16);
    layout->addLayout(form, 1);
    layout->addWidget(buttons);

    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

EventInfo EventDialog::info() const
{
    EventInfo e = m_info;
    e.name = m_nameEdit->text().trimmed();
    e.note = m_noteEdit->toPlainText().trimmed();
    if (!e.isTemplate) {
        e.date = m_dateEdit->date();
        e.time = time();
    }
    return e;
}

QTime EventDialog::time() const
{
    return QTime::fromString(m_timeCombo->currentText().trimmed(), kTimeParsing);
}

QString EventDialog::templateId() const
{
    return m_templateCombo ? m_templateCombo->currentData().toString() : QString();
}

QDate EventDialog::nextSunday()
{
    const QDate today = QDate::currentDate();
    return today.addDays((7 - today.dayOfWeek()) % 7);   // dayOfWeek: Monday = 1, Sunday = 7
}
