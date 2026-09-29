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
#include "textslidedialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

TextSlideDialog::TextSlideDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Text slides"));
    resize(560, 520);

    m_typeCombo = new QComboBox(this);
    for (MediaItem::Type t : {MediaItem::Song, MediaItem::Bible, MediaItem::Custom}) {
        m_typeCombo->addItem(MediaItem::typeIcon(t), MediaItem::typeName(t), int(t));
    }

    m_titleEdit = new QLineEdit(this);
    m_textEdit  = new QPlainTextEdit(this);

    auto *hint = new QLabel(tr("An empty line starts a new slide."), this);
    hint->setEnabled(false);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // OK only makes sense with a title
    auto updateOk = [this, buttons]() {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!m_titleEdit->text().trimmed().isEmpty());
    };
    connect(m_titleEdit, &QLineEdit::textChanged, this, updateOk);
    updateOk();

    auto *form = new QFormLayout;
    form->addRow(tr("Type:"), m_typeCombo);
    form->addRow(tr("Title:"), m_titleEdit);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(m_textEdit, 1);
    layout->addWidget(hint);
    layout->addWidget(buttons);

    m_titleEdit->setFocus();
}

void TextSlideDialog::setType(MediaItem::Type type)
{
    const int idx = m_typeCombo->findData(int(type));
    if (idx >= 0) {
        m_typeCombo->setCurrentIndex(idx);
    }
}

void TextSlideDialog::setTitle(const QString &title) { m_titleEdit->setText(title); }
void TextSlideDialog::setText(const QString &text)   { m_textEdit->setPlainText(text); }

MediaItem::Type TextSlideDialog::type() const
{
    return MediaItem::Type(m_typeCombo->currentData().toInt());
}

QString TextSlideDialog::title() const { return m_titleEdit->text().trimmed(); }
QString TextSlideDialog::text() const  { return m_textEdit->toPlainText(); }
