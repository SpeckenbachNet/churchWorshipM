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
#include "slidedeck.h"
#include "presentationconverter.h"
#include "textmarkup.h"

#include <QApplication>
#include <QPainter>
#include <QPdfDocument>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextOption>

namespace {

// ---------------------------------------------------------------------------
// Image: exactly one slide
// ---------------------------------------------------------------------------

class ImageDeck : public SlideDeck {
public:
    explicit ImageDeck(const QImage &image) : m_image(image) {}

    int count() const override { return 1; }

    QImage render(int, const QSize &maxSize) override
    {
        return m_image.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

private:
    QImage m_image;
};

// ---------------------------------------------------------------------------
// PDF: one slide per page (also used for converted presentations)
// ---------------------------------------------------------------------------

class PdfDeck : public SlideDeck {
public:
    bool load(const QString &path, QString *error)
    {
        if (m_doc.load(path) != QPdfDocument::Error::None || m_doc.pageCount() < 1) {
            *error = QApplication::translate("SlideDeck", "The PDF file could not be opened.");
            return false;
        }
        return true;
    }

    int count() const override { return m_doc.pageCount(); }

    QImage render(int index, const QSize &maxSize) override
    {
        const QSize size = m_doc.pagePointSize(index).scaled(maxSize, Qt::KeepAspectRatio).toSize();
        if (size.isEmpty()) {
            return QImage();
        }
        // Pages are rendered transparent; most PDFs have no own page background -> white paper
        QImage page(size, QImage::Format_RGB32);
        page.fill(Qt::white);
        QPainter p(&page);
        p.drawImage(0, 0, m_doc.render(index, size));
        return page;
    }

private:
    QPdfDocument m_doc;
};

// ---------------------------------------------------------------------------
// Text: songs, bible texts, own slides. An empty line starts a new slide.
// ---------------------------------------------------------------------------

class TextDeck : public SlideDeck {
public:
    explicit TextDeck(const QString &text, const QString &credits = {}, const QStringList &translation = {})
        : m_credits(credits), m_translation(translation)
    {
        static const QRegularExpression separator(QStringLiteral(R"(\n\s*\n)"));
        for (const QString &block : text.split(separator, Qt::SkipEmptyParts)) {
            if (!block.trimmed().isEmpty()) {
                m_pages << block.trimmed();
            }
        }
        if (m_pages.isEmpty()) {
            m_pages << QString();
        }
    }

    int count() const override { return int(m_pages.size()); }

    QImage render(int index, const QSize &maxSize) override
    {
        const QSize size = textSlideAspect.scaled(maxSize, Qt::KeepAspectRatio);
        if (size.isEmpty()) {
            return QImage();
        }
        return SlideBackground::compose(size, m_background, m_backgroundImage, [&](QPainter &p) {
            paint(p, index, size);
        });
    }

private:
    void paint(QPainter &p, int index, const QSize &size)
    {
        // Safe area: margins at the edges
        QRect area = QRect(QPoint(0, 0), size).adjusted(size.width() / 16, size.height() / 12,
                                                        -size.width() / 16, -size.height() / 12);
        const QColor muted = m_background.mutedColor();

        // Credits (copyright, CCLI) small at the bottom of the first slide
        if (index == 0 && !m_credits.isEmpty()) {
            QFont small = QApplication::font();
            small.setPixelSize(qMax(6, size.height() / 54));
            const QFontMetrics fm(small);
            const int lines = int(m_credits.count('\n')) + 1;
            const int height = lines * fm.lineSpacing();
            const QRect creditRect(area.left(), size.height() - size.height() / 24 - height,
                                   area.width(), height);
            p.setFont(small);
            p.setPen(muted);
            p.drawText(creditRect, Qt::AlignHCenter | Qt::AlignBottom, m_credits);
            area.setBottom(qMin(area.bottom(), creditRect.top() - fm.lineSpacing()));
        }
        // Formatted text (bold / italic / underline), every line centered
        QTextDocument doc;
        prepare(doc, m_pages.at(index), Qt::white, area.width());
        // Translation below, half the size and gray
        const QString translated = m_translation.value(index).trimmed();
        QTextDocument trans;
        prepare(trans, translated, muted, area.width());

        // Largest font size where the whole text fits; a single word is never broken
        int pixelSize = size.height() / 9;
        for (; pixelSize > 6; pixelSize -= qMax(1, pixelSize / 20)) {
            if (layout(doc, trans, translated.isEmpty(), pixelSize) <= area.height()
                && doc.idealWidth() <= area.width() && trans.idealWidth() <= area.width()) {
                break;
            }
        }
        const qreal height = layout(doc, trans, translated.isEmpty(), pixelSize);

        const qreal top = qMax<qreal>(area.top(), area.top() + (area.height() - height) / 2.0);
        p.translate(area.left(), top);
        doc.drawContents(&p);
        if (!translated.isEmpty()) {
            p.translate(0, height - trans.size().height());
            trans.drawContents(&p);
        }
    }

    static void prepare(QTextDocument &doc, const QString &markup, const QColor &color, int width)
    {
        doc.setDocumentMargin(0);
        QTextOption option(Qt::AlignHCenter);
        option.setWrapMode(QTextOption::WordWrap);
        doc.setDefaultTextOption(option);
        QTextCharFormat base;
        base.setForeground(color);
        TextMarkup::fillDocument(&doc, markup, base);
        doc.setTextWidth(width);
    }

    // Applies the font size, returns the height of lyrics + translation
    static qreal layout(QTextDocument &doc, QTextDocument &trans, bool noTranslation, int pixelSize)
    {
        QFont font = QApplication::font();
        font.setPixelSize(pixelSize);
        doc.setDefaultFont(font);
        if (noTranslation) {
            return doc.size().height();
        }
        font.setPixelSize(qMax(6, pixelSize / 2));
        trans.setDefaultFont(font);
        const qreal gap = pixelSize * 0.6;
        return doc.size().height() + gap + trans.size().height();
    }

    QStringList m_pages;
    QString     m_credits;
    QStringList m_translation;
};

// ---------------------------------------------------------------------------
// Bible passage: verses with small verse numbers, reference in the corner.
// Whole verses are distributed over the slides, a verse is never split.
// ---------------------------------------------------------------------------

class BibleDeck : public SlideDeck {
public:
    explicit BibleDeck(const BiblePassage &passage) : m_passage(passage)
    {
        paginate();
    }

    int count() const override { return int(m_pages.size()); }

    QImage render(int index, const QSize &maxSize) override
    {
        const QSize size = textSlideAspect.scaled(maxSize, Qt::KeepAspectRatio);
        if (size.isEmpty() || index < 0 || index >= count()) {
            return QImage();
        }

        return SlideBackground::compose(size, m_background, m_backgroundImage, [&](QPainter &p) {
            paint(p, index, size);
        });
    }

private:
    void paint(QPainter &p, int index, const QSize &size)
    {
        const Layout l = layoutFor(size);

        // Text: largest font size that fits
        const QString html = pageHtml(m_pages.at(index));
        QTextDocument doc;
        int pixelSize = size.height() / 11;
        for (; pixelSize > 6; pixelSize -= qMax(1, pixelSize / 20)) {
            prepare(doc, html, l.text.width(), pixelSize);
            if (doc.size().height() <= l.text.height()) {
                break;
            }
        }
        prepare(doc, html, l.text.width(), pixelSize);
        // Vertically centered in the text area
        const qreal top = l.text.top() + (l.text.height() - doc.size().height()) / 2.0;
        p.translate(l.text.left(), qMax<qreal>(l.text.top(), top));
        doc.drawContents(&p);
        p.resetTransform();

        // Reference, e.g. "Johannes 3,16–18 (NeÜ)"
        QFont font = QApplication::font();
        font.setPixelSize(qMax(8, size.height() / 28));
        p.setFont(font);
        p.setPen(m_background.mutedColor());
        QString reference = m_passage.reference();
        if (!m_passage.abbreviation.isEmpty()) {
            reference += QStringLiteral(" (%1)").arg(m_passage.abbreviation);
        }
        p.drawText(l.footer, Qt::AlignRight | Qt::AlignVCenter, reference);
    }

    struct Layout {
        QRect text;
        QRect footer;
    };

    static Layout layoutFor(const QSize &size)
    {
        const int marginX = size.width() / 16;
        const int marginY = size.height() / 14;
        const int footerH = size.height() / 12;
        const QRect inner(marginX, marginY, size.width() - 2 * marginX, size.height() - 2 * marginY);
        return {inner.adjusted(0, 0, 0, -footerH), QRect(inner.left(), inner.bottom() - footerH,
                                                         inner.width(), footerH)};
    }

    static void prepare(QTextDocument &doc, const QString &html, int width, int pixelSize)
    {
        QFont font = QApplication::font();
        font.setPixelSize(pixelSize);
        doc.setDefaultFont(font);
        doc.setDocumentMargin(0);
        doc.setDefaultStyleSheet(QStringLiteral("body { color: white; } .v { color: #9fb3c8; }"));
        doc.setHtml(html);
        doc.setTextWidth(width);
    }

    QString pageHtml(const QList<int> &indexes) const
    {
        QString html = QStringLiteral("<body>");
        for (int i : indexes) {
            QString text = m_passage.texts.value(i).toHtmlEscaped();
            text.replace('\n', QLatin1String("<br>"));
            html += QStringLiteral("<sub class=\"v\">%1</sub>&nbsp;%2 ").arg(m_passage.verses.value(i)).arg(text);
        }
        return html + QStringLiteral("</body>");
    }

    // Distributes the verses; measured on a reference slide so the result does not
    // depend on the output resolution
    void paginate()
    {
        const int verseCount = int(qMin(m_passage.verses.size(), m_passage.texts.size()));
        if (m_passage.oneVersePerSlide) {
            for (int i = 0; i < verseCount; ++i) {
                m_pages << QList<int>{i};
            }
            return;
        }

        const QSize reference(1600, 900);
        const Layout l = layoutFor(reference);
        const int minPixelSize = reference.height() / 17;   // smallest size that is still well readable

        QTextDocument doc;
        QList<int> page;
        for (int i = 0; i < verseCount; ++i) {
            QList<int> candidate = page;
            candidate << i;
            prepare(doc, pageHtml(candidate), l.text.width(), minPixelSize);
            if (!page.isEmpty() && doc.size().height() > l.text.height()) {
                m_pages << page;
                page = {i};
            } else {
                page = candidate;
            }
        }
        if (!page.isEmpty()) {
            m_pages << page;
        }
    }

    BiblePassage       m_passage;
    QList<QList<int>>  m_pages;   // indexes into verses/texts per slide
};

} // namespace

void SlideDeck::setBackground(const SlideBackground &background)
{
    m_background = background.resolved(SlideBackground());
    m_backgroundImage = m_background.kind == SlideBackground::Image ? QImage(m_background.path) : QImage();
    // Photos are often much larger than any projector: smaller once instead of on every slide
    if (m_backgroundImage.width() > 3840 || m_backgroundImage.height() > 2160) {
        m_backgroundImage = m_backgroundImage.scaled(3840, 2160, Qt::KeepAspectRatioByExpanding,
                                                     Qt::SmoothTransformation);
    }
}

std::unique_ptr<SlideDeck> SlideDeck::createBible(const BiblePassage &passage)
{
    return std::make_unique<BibleDeck>(passage);
}

std::unique_ptr<SlideDeck> SlideDeck::create(MediaItem::Type type, const QString &source,
                                             const QString &text, const QJsonObject &bible,
                                             QString *error, const QString &credits,
                                             const QStringList &translation)
{
    if (type == MediaItem::Bible && !bible.isEmpty()) {
        const BiblePassage passage = BiblePassage::fromJson(bible);
        if (passage.isValid()) {
            return createBible(passage);
        }
    }

    switch (type) {
    case MediaItem::Image: {
        QImage image(source);
        if (image.isNull()) {
            *error = QApplication::translate("SlideDeck", "The image could not be loaded.");
            return nullptr;
        }
        return std::make_unique<ImageDeck>(image);
    }
    case MediaItem::Pdf: {
        auto deck = std::make_unique<PdfDeck>();
        return deck->load(source, error) ? std::move(deck) : nullptr;
    }
    case MediaItem::PowerPoint: {
        // Converted in the background by PresentationConverter
        const QString pdf = PresentationConverter::cachedPdf(source);
        if (pdf.isEmpty()) {
            *error = QApplication::translate("SlideDeck", "The presentation has not been converted yet.");
            return nullptr;
        }
        auto deck = std::make_unique<PdfDeck>();
        return deck->load(pdf, error) ? std::move(deck) : nullptr;
    }
    case MediaItem::Song:
        return std::make_unique<TextDeck>(text, credits, translation);
    case MediaItem::Bible:
    case MediaItem::Custom:
        return std::make_unique<TextDeck>(text);
    default:
        *error = QApplication::translate("SlideDeck", "Unsupported file type.");
        return nullptr;
    }
}
