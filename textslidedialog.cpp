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
#include "richtextedit.h"
#include "slidedeck.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr QSize kPreviewSize{224, 126};   // 16:9 like the text slides

} // namespace

TextSlideDialog::TextSlideDialog(MediaItem::Type type, QWidget *parent)
    : QDialog(parent), m_type(type)
{
    switch (type) {
    case MediaItem::Song:  setWindowTitle(tr("Song text")); break;
    case MediaItem::Bible: setWindowTitle(tr("Bible text")); break;
    default:               setWindowTitle(tr("Own slide")); break;
    }
    setStyleSheet(QStringLiteral("QLineEdit { padding: 4px 6px; }"));

    m_titleEdit = new QLineEdit(this);
    m_titleEdit->setPlaceholderText(tr("Optional, otherwise the first line"));

    m_textEdit = new RichTextEdit(this);
    m_textEdit->setPlaceholderText(tr("Text of the slides.\n\nAn empty line starts a new slide."));

    // Preview: the slides as they appear on the projector
    m_preview = new QListWidget(this);
    m_preview->setViewMode(QListView::IconMode);
    m_preview->setFlow(QListView::TopToBottom);
    m_preview->setWrapping(false);
    m_preview->setMovement(QListView::Static);
    m_preview->setSelectionMode(QAbstractItemView::NoSelection);
    m_preview->setFocusPolicy(Qt::NoFocus);
    m_preview->setIconSize(kPreviewSize);
    m_preview->setSpacing(6);
    m_preview->setFixedWidth(kPreviewSize.width() + 40);

    m_count = new QLabel(this);
    m_count->setEnabled(false);   // muted

    auto *hint = new QLabel(tr("An empty line starts a new slide."), this);
    hint->setEnabled(false);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("Title:"), m_titleEdit);

    auto *textColumn = new QVBoxLayout;
    textColumn->setSpacing(6);
    textColumn->addWidget(m_textEdit, 1);
    textColumn->addWidget(hint);

    auto *previewColumn = new QVBoxLayout;
    previewColumn->setSpacing(6);
    previewColumn->addWidget(m_preview, 1);
    previewColumn->addWidget(m_count);

    auto *content = new QHBoxLayout;
    content->setSpacing(12);
    content->addLayout(textColumn, 1);
    content->addLayout(previewColumn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    layout->addLayout(form);
    layout->addLayout(content, 1);
    layout->addWidget(buttons);
    resize(860, 600);

    // Re-render shortly after typing stops, not with every key
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(150);
    connect(&m_previewTimer, &QTimer::timeout, this, &TextSlideDialog::updatePreview);
    connect(m_textEdit, &RichTextEdit::textChanged, this, [this] {
        m_previewTimer.start();
        updateOk();
    });
    connect(m_titleEdit, &QLineEdit::textChanged, this, &TextSlideDialog::updateOk);

    updatePreview();
    updateOk();
    m_textEdit->setFocus();
}

void TextSlideDialog::setTitle(const QString &title)
{
    m_titleEdit->setText(title);
}

void TextSlideDialog::setText(const QString &text)
{
    m_textEdit->setMarkup(text);
    updatePreview();
}

QString TextSlideDialog::title() const
{
    const QString title = m_titleEdit->text().trimmed();
    if (!title.isEmpty()) {
        return title;
    }
    return m_textEdit->plainText().trimmed().section('\n', 0, 0).trimmed();
}

QString TextSlideDialog::text() const
{
    return m_textEdit->markup();
}

void TextSlideDialog::updateOk()
{
    m_okBtn->setEnabled(!title().isEmpty());
}

void TextSlideDialog::updatePreview()
{
    m_preview->clear();
    const QString text = m_textEdit->markup();
    if (text.trimmed().isEmpty()) {
        m_count->setText(tr("No slides yet"));
        return;
    }
    QString error;
    const auto deck = SlideDeck::create(MediaItem::Custom, {}, text, QJsonObject(), &error);
    if (!deck) {
        m_count->setText(error);
        return;
    }
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < deck->count(); ++i) {
        QImage image = deck->render(i, kPreviewSize * dpr);
        image.setDevicePixelRatio(dpr);
        m_preview->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(image)), QString()));
    }
    m_count->setText(tr("%n slide(s)", "", deck->count()));
}
