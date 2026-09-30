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
#include "richtextedit.h"
#include "textmarkup.h"

#include <QAction>
#include <QHBoxLayout>
#include <QMimeData>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Pasting keeps bold / italic / underline only: slides stay uniform in font and color
class SanitizingTextEdit : public QTextEdit {
public:
    using QTextEdit::QTextEdit;

protected:
    void insertFromMimeData(const QMimeData *source) override
    {
        QTextDocument pasted;
        if (source->hasHtml()) {
            pasted.setHtml(source->html());
        } else if (source->hasText()) {
            pasted.setPlainText(source->text());
        } else {
            return;
        }
        const QString markup = TextMarkup::fromDocument(&pasted).trimmed();

        QTextDocument clean;
        TextMarkup::fillDocument(&clean, markup, QTextCharFormat());
        QTextCursor cursor = textCursor();
        cursor.insertFragment(QTextDocumentFragment(&clean));
        setTextCursor(cursor);
    }
};

} // namespace

RichTextEdit::RichTextEdit(QWidget *parent)
    : QWidget(parent)
{
    m_edit = new SanitizingTextEdit(this);
    m_edit->setAcceptRichText(true);
    m_edit->document()->setDocumentMargin(8);
    setFocusProxy(m_edit);

    // Buttons show the letters in the language of the user, e.g. F K U in German
    auto makeButton = [this](const QString &letter, const QString &tip, const QKeySequence &key,
                             void (*style)(QFont &)) {
        auto *btn = new QToolButton(this);
        btn->setText(letter);
        QFont font = btn->font();
        style(font);
        btn->setFont(font);
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setFocusPolicy(Qt::NoFocus);   // typing continues in the text
        btn->setFixedSize(30, 26);
        btn->setToolTip(QStringLiteral("%1 (%2)").arg(tip, key.toString(QKeySequence::NativeText)));

        auto *action = new QAction(this);
        action->setShortcut(key);
        action->setShortcutContext(Qt::WidgetShortcut);
        m_edit->addAction(action);
        connect(action, &QAction::triggered, btn, &QToolButton::click);
        return btn;
    };
    m_boldBtn = makeButton(tr("B", "button: bold"), tr("Bold"), QKeySequence::Bold,
                           [](QFont &f) { f.setBold(true); });
    m_italicBtn = makeButton(tr("I", "button: italic"), tr("Italic"), QKeySequence::Italic,
                             [](QFont &f) { f.setItalic(true); });
    m_underlineBtn = makeButton(tr("U", "button: underline"), tr("Underline"), QKeySequence::Underline,
                                [](QFont &f) { f.setUnderline(true); });

    connect(m_boldBtn, &QToolButton::clicked, this, [this](bool on) {
        QTextCharFormat f;
        f.setFontWeight(on ? QFont::Bold : QFont::Normal);
        m_edit->mergeCurrentCharFormat(f);
    });
    connect(m_italicBtn, &QToolButton::clicked, this, [this](bool on) {
        QTextCharFormat f;
        f.setFontItalic(on);
        m_edit->mergeCurrentCharFormat(f);
    });
    connect(m_underlineBtn, &QToolButton::clicked, this, [this](bool on) {
        QTextCharFormat f;
        f.setFontUnderline(on);
        m_edit->mergeCurrentCharFormat(f);
    });
    connect(m_edit, &QTextEdit::currentCharFormatChanged, this, &RichTextEdit::updateButtons);
    connect(m_edit, &QTextEdit::textChanged, this, &RichTextEdit::textChanged);

    auto *bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(2);
    bar->addWidget(m_boldBtn);
    bar->addWidget(m_italicBtn);
    bar->addWidget(m_underlineBtn);
    bar->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    layout->addLayout(bar);
    layout->addWidget(m_edit, 1);
}

void RichTextEdit::setMarkup(const QString &markup)
{
    TextMarkup::fillDocument(m_edit->document(), markup, QTextCharFormat());
    m_edit->moveCursor(QTextCursor::Start);
    updateButtons();
}

QString RichTextEdit::markup() const
{
    return TextMarkup::fromDocument(m_edit->document());
}

QString RichTextEdit::plainText() const
{
    return m_edit->toPlainText();
}

void RichTextEdit::setPlaceholderText(const QString &text)
{
    m_edit->setPlaceholderText(text);
}

void RichTextEdit::updateButtons()
{
    const QTextCharFormat f = m_edit->currentCharFormat();
    m_boldBtn->setChecked(f.fontWeight() >= QFont::DemiBold);
    m_italicBtn->setChecked(f.fontItalic());
    m_underlineBtn->setChecked(f.fontUnderline());
}
