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
#include "printplan.h"
#include "countdown.h"
#include "mediaitem.h"
#include "song.h"
#include "songstore.h"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QJsonObject>
#include <QPrintPreviewWidget>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QFileDialog>
#include <QLabel>
#include <QLocale>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocument>
#include <QVBoxLayout>

namespace {

// Lyrics markup (b / i / u, entities escaped) is valid HTML; an empty line between the slides
QString lyricsHtml(const QString &markup)
{
    static const QRegularExpression slideBreak(QStringLiteral(R"(\n\s*\n)"));
    QStringList slides;
    for (const QString &slide : markup.split(slideBreak, Qt::SkipEmptyParts)) {
        slides << slide.trimmed().replace(QLatin1Char('\n'), QLatin1String("<br>"));
    }
    return QStringLiteral("<p class=\"text\">%1</p>").arg(slides.join(QLatin1String("<br><br>")));
}

// "16 Denn also ..." -> small verse numbers
QString bibleHtml(const QString &text)
{
    static const QRegularExpression verse(QStringLiteral(R"(^(\d+)\s+)"));
    QStringList lines;
    for (const QString &line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        QString html = line.toHtmlEscaped();
        html.replace(verse, QStringLiteral("<sup>\\1</sup>&nbsp;"));
        lines << html;
    }
    return QStringLiteral("<p class=\"text\">%1</p>").arg(lines.join(QLatin1Char(' ')));
}

// "Vers 1 · Refrain · Vers 3 (Englisch)" from the order of the entry (or the song's default)
QString songOrder(const QJsonObject &songObject, const SongStore *songs)
{
    const Song song = songs ? songs->song(songObject.value("id").toString()) : Song();
    if (!song.isValid()) {
        return {};
    }
    QStringList order;
    for (const QJsonValue &v : songObject.value("order").toArray()) {
        order << v.toString();
    }
    if (order.isEmpty()) {
        order = song.order;
    }
    QStringList labels;
    for (const QString &id : std::as_const(order)) {
        QString songId;
        const QString partId = Song::splitPartId(id, &songId);
        const Song source = songId.isEmpty() ? song : songs->song(songId);
        const int index = source.partIndex(partId);
        if (index < 0) {
            continue;
        }
        const QString label = source.parts.at(index).label();
        labels << (songId.isEmpty() ? label
                                    : QStringLiteral("%1 (%2)").arg(label, Song::languageName(source.language)));
    }
    return labels.join(QStringLiteral(" · "));
}

} // namespace

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

PrintOptions PrintOptions::load()
{
    const QSettings settings;
    PrintOptions o;
    o.songOrder = settings.value("print/songOrder", false).toBool();
    o.lyrics = settings.value("print/lyrics", false).toBool();
    o.bibleText = settings.value("print/bibleText", false).toBool();
    return o;
}

void PrintOptions::save() const
{
    QSettings settings;
    settings.setValue("print/songOrder", songOrder);
    settings.setValue("print/lyrics", lyrics);
    settings.setValue("print/bibleText", bibleText);
}

PrintPlanDialog::PrintPlanDialog(const EventInfo &event, const QJsonArray &items, const SongStore *songs,
                                 QWidget *parent)
    : QDialog(parent), m_event(event), m_items(items), m_songs(songs)
{
    setWindowTitle(tr("Print plan"));
    m_printer.setDocName(event.name);

    const PrintOptions o = PrintOptions::load();
    m_order = new QCheckBox(tr("Order of the song parts"), this);
    m_lyrics = new QCheckBox(tr("Lyrics"), this);
    m_bible = new QCheckBox(tr("Bible texts"), this);
    m_order->setChecked(o.songOrder);
    m_lyrics->setChecked(o.lyrics);
    m_bible->setChecked(o.bibleText);

    auto *hint = new QLabel(tr("Every entry gets one line, songs are numbered. Optionally also:"), this);
    hint->setWordWrap(true);
    m_pages = new QLabel(this);
    m_pages->setEnabled(false);   // muted

    auto *zoomIn = new QPushButton(QStringLiteral("+"), this);
    auto *zoomOut = new QPushButton(QStringLiteral("\u2212"), this);
    auto *fit = new QPushButton(tr("Whole page"), this);
    for (QPushButton *btn : {zoomIn, zoomOut}) {
        btn->setFixedWidth(40);
    }
    auto *zoom = new QHBoxLayout;
    zoom->addWidget(zoomOut);
    zoom->addWidget(zoomIn);
    zoom->addWidget(fit, 1);

    auto *sideBox = new QWidget(this);
    sideBox->setFixedWidth(240);
    auto *side = new QVBoxLayout(sideBox);
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(8);
    side->addWidget(hint);
    side->addSpacing(4);
    side->addWidget(m_order);
    side->addWidget(m_lyrics);
    side->addWidget(m_bible);
    side->addStretch();
    side->addWidget(m_pages);
    side->addLayout(zoom);

    m_preview = new QPrintPreviewWidget(&m_printer, this);
    m_preview->setZoomMode(QPrintPreviewWidget::FitInView);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *printBtn = buttons->addButton(tr("Print..."), QDialogButtonBox::AcceptRole);
    QPushButton *pdfBtn = buttons->addButton(tr("Save as PDF..."), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    printBtn->setDefault(true);

    auto *content = new QHBoxLayout;
    content->setSpacing(16);
    content->addWidget(sideBox);
    content->addWidget(m_preview, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(16);
    layout->addLayout(content, 1);
    layout->addWidget(buttons);
    resize(1000, 760);

    connect(m_preview, &QPrintPreviewWidget::paintRequested, this, &PrintPlanDialog::printTo);
    connect(m_preview, &QPrintPreviewWidget::previewChanged, this, [this] {
        m_pages->setText(tr("%n page(s)", "", m_preview->pageCount()));
    });
    for (QCheckBox *box : {m_order, m_lyrics, m_bible}) {
        connect(box, &QCheckBox::toggled, this, [this] {
            options().save();
            m_preview->updatePreview();
        });
    }
    connect(zoomIn, &QPushButton::clicked, m_preview, [this] { m_preview->zoomIn(); });
    connect(zoomOut, &QPushButton::clicked, m_preview, [this] { m_preview->zoomOut(); });
    connect(fit, &QPushButton::clicked, m_preview, &QPrintPreviewWidget::fitInView);
    connect(printBtn, &QPushButton::clicked, this, &PrintPlanDialog::print);
    connect(pdfBtn, &QPushButton::clicked, this, &PrintPlanDialog::savePdf);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

PrintOptions PrintPlanDialog::options() const
{
    PrintOptions o;
    o.songOrder = m_order->isChecked();
    o.lyrics = m_lyrics->isChecked();
    o.bibleText = m_bible->isChecked();
    return o;
}

void PrintPlanDialog::printTo(QPrinter *printer)
{
    // Without a page size the document is laid out on the paper and gets page numbers
    QTextDocument doc;
    doc.setHtml(PrintPlan::html(m_event, m_items, options(), m_songs));
    doc.print(printer);
}

void PrintPlanDialog::print()
{
    QPrintDialog dialog(&m_printer, this);
    dialog.setWindowTitle(tr("Print plan"));
    if (dialog.exec() == QDialog::Accepted) {
        printTo(&m_printer);
        accept();
    }
}

void PrintPlanDialog::savePdf()
{
    QSettings settings;
    QString name = m_event.name;
    name.replace(QRegularExpression(QStringLiteral(R"([/\\:*?"<>|])")), QStringLiteral("-"));
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save as PDF"), settings.value("print/pdfDir").toString() + QLatin1Char('/') + name + ".pdf",
        tr("PDF file (*.pdf)"));
    if (path.isEmpty()) {
        return;
    }
    settings.setValue("print/pdfDir", QFileInfo(path).absolutePath());
    QPrinter pdf(QPrinter::HighResolution);
    pdf.setOutputFormat(QPrinter::PdfFormat);
    pdf.setOutputFileName(path);
    pdf.setPageLayout(m_printer.pageLayout());
    pdf.setDocName(m_event.name);
    printTo(&pdf);
    accept();
}

// ---------------------------------------------------------------------------
// Plan
// ---------------------------------------------------------------------------

namespace PrintPlan {

QString html(const EventInfo &event, const QJsonArray &items, const PrintOptions &options, const SongStore *songs)
{
    const QLocale locale;
    QString html = QStringLiteral(
        "<html><head><style>"
        // Explicit colors: paper is white, also when the app runs in dark mode
        "body { font-size: 11pt; color: #000; }"
        "h1 { font-size: 18pt; margin-bottom: 2pt; color: #000; }"
        "p.when { color: #555; margin-top: 0; }"
        "p.note { margin-top: 6pt; margin-bottom: 12pt; }"
        "td.kind { color: #555; }"
        "td.title { font-weight: bold; }"
        "p.detail { color: #555; margin: 2pt 0 0 0; font-weight: normal; }"
        "p.text { margin: 4pt 0 0 0; font-weight: normal; }"
        "p.printed { color: #888; font-size: 8pt; }"
        "</style></head><body>");

    html += QStringLiteral("<h1>%1</h1>").arg(event.name.toHtmlEscaped());
    if (!event.isTemplate) {
        QString when = locale.toString(event.date, QLocale::LongFormat);
        if (event.time.isValid()) {
            when += QStringLiteral(" · ") + QApplication::translate("PrintPlan", "%1 h")
                                                    .arg(locale.toString(event.time, QLocale::ShortFormat));
        }
        html += QStringLiteral("<p class=\"when\">%1</p>").arg(when.toHtmlEscaped());
    }
    if (!event.note.isEmpty()) {
        html += QStringLiteral("<p class=\"note\">%1</p>")
                    .arg(event.note.toHtmlEscaped().replace(QLatin1Char('\n'), QLatin1String("<br>")));
    }

    // Qt's rich text ignores CSS padding in tables: spacing via the attributes
    html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"4\" width=\"100%\">");
    int songNumber = 0;
    for (const QJsonValue &v : items) {
        const QJsonObject item = v.toObject();
        const auto type = MediaItem::typeFromKey(item.value("type").toString());
        QString kind = MediaItem::typeName(type);
        QString title = item.value("title").toString().toHtmlEscaped();
        QString more;

        switch (type) {
        case MediaItem::Song: {
            kind = QApplication::translate("PrintPlan", "Song %1:").arg(++songNumber);
            const QJsonObject songObject = item.value("song").toObject();
            if (options.songOrder) {
                const QString order = songOrder(songObject, songs);
                if (!order.isEmpty()) {
                    more += QStringLiteral("<p class=\"detail\">%1</p>").arg(order.toHtmlEscaped());
                }
            }
            if (options.lyrics) {
                more += lyricsHtml(item.value("text").toString());
            }
            break;
        }
        case MediaItem::Bible:
            kind += QLatin1Char(':');
            if (options.bibleText) {
                more += bibleHtml(item.value("text").toString());
            }
            break;
        case MediaItem::Countdown:
            kind += QLatin1Char(':');
            title = CountdownSettings::fromJson(item.value("countdown").toObject()).summary().toHtmlEscaped();
            break;
        case MediaItem::Blank:
            title.clear();   // "Blank" alone says it all
            break;
        default:
            kind += QLatin1Char(':');
            break;
        }
        html += QStringLiteral("<tr><td class=\"kind\" width=\"18%\" valign=\"top\">%1</td>"
                               "<td class=\"title\" valign=\"top\">%2%3</td></tr>")
                    .arg(kind.toHtmlEscaped(), title, more);
    }
    html += QStringLiteral("</table>");
    html += QStringLiteral("<p class=\"printed\">%1</p>")
                .arg(QApplication::translate("PrintPlan", "Printed on %1")
                         .arg(locale.toString(QDateTime::currentDateTime(), QLocale::ShortFormat)).toHtmlEscaped());
    return html + QStringLiteral("</body></html>");
}

void print(QWidget *parent, const EventInfo &event, const QJsonArray &items, const SongStore *songs)
{
    PrintPlanDialog dialog(event, items, songs, parent);
    dialog.exec();
}

} // namespace PrintPlan
