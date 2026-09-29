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
#include "biblepage.h"
#include "biblebooks.h"
#include "flowlayout.h"
#include "slidedeck.h"

#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStackedLayout>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kVerseRole   = Qt::UserRole + 1;
constexpr int kHeadingRole = Qt::UserRole + 2;
constexpr QSize kTileGrid{46, 38};
constexpr QSize kPreviewSize{160, 90};

// Book groups by genre, each with its own color
struct BookGroup {
    const char *name;
    int         first;
    int         last;
    QRgb        color;
};

const BookGroup kOldTestament[] = {
    {QT_TRANSLATE_NOOP("BiblePage", "Law"),            1,  5, 0xc9973f},
    {QT_TRANSLATE_NOOP("BiblePage", "History"),        6, 17, 0x5f9e5f},
    {QT_TRANSLATE_NOOP("BiblePage", "Poetry"),        18, 22, 0x9b6bc0},
    {QT_TRANSLATE_NOOP("BiblePage", "Major prophets"), 23, 27, 0xc8605a},
    {QT_TRANSLATE_NOOP("BiblePage", "Minor prophets"), 28, 39, 0xcf7d4f},
};
const BookGroup kNewTestament[] = {
    {QT_TRANSLATE_NOOP("BiblePage", "Gospels"),         40, 43, 0x4a8fd0},
    {QT_TRANSLATE_NOOP("BiblePage", "History"),         44, 44, 0x5f9e5f},
    {QT_TRANSLATE_NOOP("BiblePage", "Letters of Paul"), 45, 57, 0x3fa39a},
    {QT_TRANSLATE_NOOP("BiblePage", "General letters"), 58, 65, 0x6f82cf},
    {QT_TRANSLATE_NOOP("BiblePage", "Prophecy"),        66, 66, 0xc8605a},
};
const BookGroup kApocrypha[] = {
    {QT_TRANSLATE_NOOP("BiblePage", "Apocrypha"), 67, 76, 0x8a8a8a},
};

QString bookTileStyle(QColor c)
{
    const QString rgb = QStringLiteral("%1,%2,%3").arg(c.red()).arg(c.green()).arg(c.blue());
    return QStringLiteral(
        "QToolButton { background: rgba(%1,60); border: 1px solid rgba(%1,150); border-radius: 6px; }"
        "QToolButton:hover { background: rgba(%1,120); }"
        "QToolButton:checked { background: rgb(%1); color: white; font-weight: bold; }"
        "QToolButton:disabled { background: transparent; border: 1px solid rgba(128,128,128,50);"
        " color: rgba(128,128,128,110); }").arg(rgb);
}

const char *kTileListStyle =
    "QListWidget { background: transparent; border: none; outline: none; }"
    "QListWidget::item { border: 1px solid palette(mid); border-radius: 6px; margin: 2px; }"
    "QListWidget::item:hover { background: rgba(128,128,128,60); }"
    "QListWidget::item:selected { background: palette(highlight); color: palette(highlighted-text);"
    " border-color: palette(highlight); }";

void setupTileList(QListWidget *list, QAbstractItemView::SelectionMode mode, int maxRows)
{
    list->setViewMode(QListView::ListMode);
    list->setFlow(QListView::LeftToRight);
    list->setWrapping(true);
    list->setResizeMode(QListView::Adjust);
    list->setUniformItemSizes(true);
    list->setGridSize(kTileGrid);
    list->setSelectionMode(mode);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setStyleSheet(QString::fromLatin1(kTileListStyle));
    list->setProperty("maxRows", maxRows);
}

QListWidgetItem *tileItem(int number)
{
    auto *item = new QListWidgetItem(QString::number(number));
    item->setTextAlignment(Qt::AlignCenter);
    item->setSizeHint(kTileGrid - QSize(4, 4));
    item->setData(kVerseRole, number);
    return item;
}

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    QFont f = label->font();
    f.setBold(true);
    label->setFont(f);
    label->setEnabled(false);   // muted color from the palette
    return label;
}

// Verse text: small number, text wrapped next to it, section heading above
class VerseDelegate : public QStyledItemDelegate {
public:
    explicit VerseDelegate(QListView *view) : QStyledItemDelegate(view), m_view(view) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);
        opt.text.clear();
        const QWidget *widget = opt.widget;
        QStyle *style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        painter->save();
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor textColor = opt.palette.color(QPalette::Normal, selected ? QPalette::HighlightedText
                                                                              : QPalette::Text);
        QColor muted = textColor;
        muted.setAlphaF(0.6);

        const QRect r = opt.rect.adjusted(kPad, kPad, -kPad, -kPad);
        int y = r.top();

        const QString heading = index.data(kHeadingRole).toString();
        if (!heading.isEmpty()) {
            const QFont hf = headingFont(opt.font);
            const QRect hr(r.left() + kNumberWidth, y, r.width() - kNumberWidth, 10000);
            painter->setFont(hf);
            painter->setPen(muted);
            const QRect used = painter->boundingRect(hr, Qt::TextWordWrap, heading);
            painter->drawText(hr, Qt::TextWordWrap, heading);
            y += used.height() + kHeadingGap;
        }

        painter->setFont(numberFont(opt.font));
        painter->setPen(muted);
        painter->drawText(QRect(r.left(), y, kNumberWidth - 8, QFontMetrics(opt.font).height()),
                          Qt::AlignRight | Qt::AlignVCenter, index.data(kVerseRole).toString());

        painter->setFont(opt.font);
        painter->setPen(textColor);
        painter->drawText(QRect(r.left() + kNumberWidth, y, r.width() - kNumberWidth, 10000),
                          Qt::TextWordWrap, index.data(Qt::DisplayRole).toString());
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const int width = qMax(100, m_view->viewport()->width());
        const int textWidth = width - 2 * kPad - kNumberWidth;
        int h = QFontMetrics(option.font).boundingRect(QRect(0, 0, textWidth, 10000), Qt::TextWordWrap,
                                                       index.data(Qt::DisplayRole).toString()).height();
        const QString heading = index.data(kHeadingRole).toString();
        if (!heading.isEmpty()) {
            h += QFontMetrics(headingFont(option.font)).boundingRect(QRect(0, 0, textWidth, 10000),
                                                                     Qt::TextWordWrap, heading).height()
                 + kHeadingGap;
        }
        return QSize(width - 2, h + 2 * kPad);
    }

private:
    static QFont headingFont(QFont f) { f.setItalic(true); f.setBold(true); return f; }
    static QFont numberFont(QFont f) { f.setBold(true); f.setPointSizeF(f.pointSizeF() * 0.8); return f; }

    static constexpr int kPad = 6;
    static constexpr int kNumberWidth = 40;
    static constexpr int kHeadingGap = 4;
    QListView *m_view;
};

} // namespace

BiblePage::BiblePage(QWidget *parent)
    : QWidget(parent)
{
    m_stack = new QStackedLayout(this);

    // --- Empty state: no bible installed
    m_emptyPage = new QWidget(this);
    auto *emptyLabel = new QLabel(tr("No bible is installed yet.<br><a href=\"#\">Open the settings</a> "
                                     "to download or import one."), m_emptyPage);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
    connect(emptyLabel, &QLabel::linkActivated, this, &BiblePage::settingsRequested);
    auto *emptyLayout = new QVBoxLayout(m_emptyPage);
    emptyLayout->addWidget(emptyLabel);
    m_stack->addWidget(m_emptyPage);

    // --- Browser
    m_browserPage = new QWidget(this);
    m_stack->addWidget(m_browserPage);

    // Left: translation + book tiles
    auto *left = new QWidget(m_browserPage);
    m_bibleCombo = new QComboBox(left);
    auto *bookScroll = new QScrollArea(left);
    bookScroll->setWidgetResizable(true);
    bookScroll->setFrameShape(QFrame::NoFrame);
    bookScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bookContainer = new QWidget(bookScroll);
    m_bookLayout = new QVBoxLayout(m_bookContainer);
    m_bookLayout->setContentsMargins(0, 0, 6, 0);
    bookScroll->setWidget(m_bookContainer);
    m_bookGroup = new QButtonGroup(this);
    m_bookGroup->setExclusive(true);

    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(sectionLabel(tr("Translation"), left));
    leftLayout->addWidget(m_bibleCombo);
    leftLayout->addSpacing(6);
    leftLayout->addWidget(bookScroll, 1);

    // Right: chapter tiles, verse tiles, verse text
    auto *right = new QWidget(m_browserPage);
    m_bookTitle = new QLabel(right);
    QFont titleFont = m_bookTitle->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.5);
    titleFont.setBold(true);
    m_bookTitle->setFont(titleFont);

    m_chapterList = new QListWidget(right);
    setupTileList(m_chapterList, QAbstractItemView::SingleSelection, 3);
    m_verseTiles = new QListWidget(right);
    setupTileList(m_verseTiles, QAbstractItemView::ExtendedSelection, 3);

    m_verseText = new QListWidget(right);
    m_verseText->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_verseText->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_verseText->setResizeMode(QListView::Adjust);
    m_verseText->setWordWrap(true);
    m_verseText->setItemDelegate(new VerseDelegate(m_verseText));

    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_bookTitle);
    rightLayout->addWidget(sectionLabel(tr("Chapter"), right));
    rightLayout->addWidget(m_chapterList);
    rightLayout->addWidget(sectionLabel(tr("Verses – click, drag, Shift+click or Ctrl+click"), right));
    rightLayout->addWidget(m_verseTiles);
    rightLayout->addWidget(m_verseText, 1);

    auto *splitter = new QSplitter(Qt::Horizontal, m_browserPage);
    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({300, 900});
    splitter->setChildrenCollapsible(false);

    // Bottom: summary, slide split, slide preview
    m_summary = new QLabel(m_browserPage);
    QFont summaryFont = m_summary->font();
    summaryFont.setBold(true);
    m_summary->setFont(summaryFont);
    m_splitCombo = new QComboBox(m_browserPage);
    m_splitCombo->addItem(tr("Split automatically"), false);
    m_splitCombo->addItem(tr("One verse per slide"), true);

    m_previewList = new QListWidget(m_browserPage);
    m_previewList->setViewMode(QListView::IconMode);
    m_previewList->setFlow(QListView::LeftToRight);
    m_previewList->setWrapping(false);
    m_previewList->setMovement(QListView::Static);
    m_previewList->setIconSize(kPreviewSize);
    m_previewList->setSpacing(4);
    m_previewList->setSelectionMode(QAbstractItemView::NoSelection);
    m_previewList->setFocusPolicy(Qt::NoFocus);
    m_previewList->setFixedHeight(kPreviewSize.height() + fontMetrics().height() + 28);
    m_previewList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *summaryRow = new QHBoxLayout;
    summaryRow->addWidget(m_summary, 1);
    summaryRow->addWidget(m_splitCombo);

    auto *browserLayout = new QVBoxLayout(m_browserPage);
    browserLayout->setContentsMargins(0, 6, 0, 0);
    browserLayout->addWidget(splitter, 1);
    browserLayout->addLayout(summaryRow);
    browserLayout->addWidget(m_previewList);

    // --- Behaviour
    connect(m_bibleCombo, &QComboBox::currentIndexChanged, this, &BiblePage::openBible);
    connect(m_bookGroup, &QButtonGroup::idClicked, this, [this](int book) { selectBook(book); });
    connect(m_chapterList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (item->data(kVerseRole).toInt() != m_chapter) {
            loadChapter(item->data(kVerseRole).toInt());
        }
    });

    // Tiles and text show the same selection
    connect(m_verseTiles, &QListWidget::itemSelectionChanged, this, [this] {
        syncSelection(m_verseTiles, m_verseText);
    });
    connect(m_verseText, &QListWidget::itemSelectionChanged, this, [this] {
        syncSelection(m_verseText, m_verseTiles);
    });
    // Clicking a tile brings the verse into view (helps with long chapters like Psalm 119)
    connect(m_verseTiles, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) m_verseText->scrollToItem(m_verseText->item(row), QAbstractItemView::PositionAtCenter);
    });
    connect(m_verseText, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row >= 0) m_verseTiles->scrollToItem(m_verseTiles->item(row));
    });
    connect(m_splitCombo, &QComboBox::currentIndexChanged, this, &BiblePage::updateSummary);

    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(150);
    connect(&m_previewTimer, &QTimer::timeout, this, &BiblePage::updatePreview);

    m_chapterList->installEventFilter(this);
    m_verseTiles->installEventFilter(this);

    reloadBibles();
}

BiblePage::~BiblePage() = default;

// ---------------------------------------------------------------------------
// Bibles
// ---------------------------------------------------------------------------

void BiblePage::reloadBibles()
{
    const QString current = m_bible ? m_bible->info().id : QSettings().value("lastBible").toString();
    const QList<BibleInfo> bibles = BibleStore::installed();

    {
        const QSignalBlocker blocker(m_bibleCombo);
        m_bibleCombo->clear();
        for (const BibleInfo &info : bibles) {
            m_bibleCombo->addItem(info.abbreviation.isEmpty()
                                      ? info.name : QStringLiteral("%1 – %2").arg(info.abbreviation, info.name),
                                  info.id);
        }
    }

    if (bibles.isEmpty()) {
        m_bible.reset();
        m_stack->setCurrentWidget(m_emptyPage);
        emit selectionChanged();
        return;
    }
    m_stack->setCurrentWidget(m_browserPage);

    const int index = qMax(0, m_bibleCombo->findData(current));
    {
        const QSignalBlocker blocker(m_bibleCombo);
        m_bibleCombo->setCurrentIndex(index);
    }
    openBible(index);
}

void BiblePage::openBible(int comboIndex)
{
    const QString id = m_bibleCombo->itemData(comboIndex).toString();
    if (id.isEmpty()) {
        return;
    }
    // Keep the selected verses when switching the translation
    const BiblePassage before = passage();

    m_bible = std::make_unique<Bible>();
    if (!m_bible->open(id)) {
        m_bible.reset();
        return;
    }
    QSettings().setValue("lastBible", id);

    buildBookTiles();
    if (m_book > 0 && m_bookButtons.value(m_book) && m_bookButtons.value(m_book)->isEnabled()) {
        selectBook(m_book, m_chapter);
        if (before.isValid() && !before.wholeChapter) {
            BiblePassage keep = before;
            keep.bibleId = id;
            setPassage(keep);
        }
    } else {
        m_book = m_chapter = 0;
        m_bookTitle->setText(tr("Choose a book"));
        m_chapterList->clear();
        m_verseTiles->clear();
        m_verseText->clear();
        updateSummary();
    }
}

void BiblePage::buildBookTiles()
{
    // Remove the old tiles
    while (QLayoutItem *item = m_bookLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_bookButtons.clear();

    const QList<int> available = m_bible ? m_bible->books() : QList<int>();

    auto addTestament = [&](const QString &title, const BookGroup *groups, int count) {
        bool any = false;
        for (int g = 0; g < count; ++g) {
            for (int nr = groups[g].first; nr <= groups[g].last; ++nr) {
                any = any || available.contains(nr);
            }
        }
        if (!any) {
            return;
        }
        auto *testament = sectionLabel(title.toUpper(), m_bookContainer);
        m_bookLayout->addWidget(testament);

        for (int g = 0; g < count; ++g) {
            const QColor color(groups[g].color);
            auto *groupLabel = new QLabel(tr(groups[g].name), m_bookContainer);
            groupLabel->setStyleSheet(QStringLiteral("QLabel { border-left: 4px solid %1; padding-left: 6px; }")
                                          .arg(color.name()));
            m_bookLayout->addWidget(groupLabel);

            auto *tiles = new QWidget(m_bookContainer);
            auto *flow = new FlowLayout(tiles, 4);
            const QString style = bookTileStyle(color);
            for (int nr = groups[g].first; nr <= groups[g].last; ++nr) {
                auto *btn = new QToolButton(tiles);
                btn->setText(BibleBooks::germanAbbreviation(nr));
                btn->setCheckable(true);
                btn->setFixedSize(56, 30);
                btn->setStyleSheet(style);
                btn->setEnabled(available.contains(nr));
                btn->setToolTip(m_bible ? m_bible->bookName(nr) : QString());
                if (btn->toolTip().isEmpty()) btn->setToolTip(BibleBooks::germanName(nr));
                flow->addWidget(btn);
                m_bookGroup->addButton(btn, nr);
                m_bookButtons.insert(nr, btn);
            }
            m_bookLayout->addWidget(tiles);
        }
        m_bookLayout->addSpacing(8);
    };

    addTestament(tr("Old Testament"), kOldTestament, int(std::size(kOldTestament)));
    addTestament(tr("New Testament"), kNewTestament, int(std::size(kNewTestament)));
    addTestament(tr("Apocrypha"), kApocrypha, int(std::size(kApocrypha)));
    m_bookLayout->addStretch(1);
}

// ---------------------------------------------------------------------------
// Book / chapter / verses
// ---------------------------------------------------------------------------

void BiblePage::selectBook(int book, int chapter)
{
    if (!m_bible) {
        return;
    }
    m_book = book;
    if (QToolButton *btn = m_bookButtons.value(book)) {
        btn->setChecked(true);
    }
    m_bookTitle->setText(m_bible->bookName(book));

    const int chapters = m_bible->chapterCount(book);
    m_chapterList->clear();
    for (int c = 1; c <= chapters; ++c) {
        m_chapterList->addItem(tileItem(c));
    }
    fitTileList(m_chapterList);

    loadChapter(qBound(1, chapter, qMax(1, chapters)));
}

void BiblePage::loadChapter(int chapter)
{
    m_chapter = chapter;
    for (int i = 0; i < m_chapterList->count(); ++i) {
        if (m_chapterList->item(i)->data(kVerseRole).toInt() == chapter) {
            m_chapterList->setCurrentRow(i);
            m_chapterList->scrollToItem(m_chapterList->item(i));
        }
    }

    const QSignalBlocker tilesBlocker(m_verseTiles);
    const QSignalBlocker textBlocker(m_verseText);
    m_verseTiles->clear();
    m_verseText->clear();

    for (const BibleVerse &v : m_bible->verses(m_book, chapter)) {
        m_verseTiles->addItem(tileItem(v.verse));
        auto *textItem = new QListWidgetItem(v.text);
        textItem->setData(kVerseRole, v.verse);
        textItem->setData(kHeadingRole, v.heading);
        m_verseText->addItem(textItem);
    }
    fitTileList(m_verseTiles);
    m_verseText->scrollToTop();
    updateSummary();
}

void BiblePage::syncSelection(QListWidget *from, QListWidget *to)
{
    if (m_syncing) {
        return;
    }
    m_syncing = true;
    QItemSelection selection;
    for (int i = 0; i < from->count() && i < to->count(); ++i) {
        if (from->item(i)->isSelected()) {
            const QModelIndex idx = to->model()->index(i, 0);
            selection.select(idx, idx);
        }
    }
    to->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    m_syncing = false;
    updateSummary();
}

BiblePassage BiblePage::passage() const
{
    BiblePassage p;
    if (!m_bible || m_book <= 0 || m_chapter <= 0 || m_verseText->count() == 0) {
        return p;
    }
    p.bibleId = m_bible->info().id;
    p.abbreviation = m_bible->info().abbreviation;
    p.bookName = m_bible->bookName(m_book);
    p.book = m_book;
    p.chapter = m_chapter;
    p.oneVersePerSlide = m_splitCombo->currentData().toBool();

    bool anySelected = false;
    for (int i = 0; i < m_verseText->count(); ++i) {
        anySelected = anySelected || m_verseText->item(i)->isSelected();
    }
    p.wholeChapter = !anySelected;   // nothing selected = whole chapter

    for (int i = 0; i < m_verseText->count(); ++i) {
        const QListWidgetItem *item = m_verseText->item(i);
        if (p.wholeChapter || item->isSelected()) {
            p.verses << item->data(kVerseRole).toInt();
            p.texts << item->text();
        }
    }
    return p;
}

void BiblePage::setPassage(const BiblePassage &passage)
{
    // Translation of the entry, if it is (still) installed
    const int index = m_bibleCombo->findData(passage.bibleId);
    if (index >= 0 && index != m_bibleCombo->currentIndex()) {
        const QSignalBlocker blocker(m_bibleCombo);
        m_bibleCombo->setCurrentIndex(index);
        openBible(index);
    }
    if (!m_bible || !m_bookButtons.value(passage.book) || !m_bookButtons.value(passage.book)->isEnabled()) {
        return;
    }
    if (passage.book != m_book || passage.chapter != m_chapter) {
        selectBook(passage.book, passage.chapter);
    }

    {
        const QSignalBlocker blocker(m_splitCombo);
        m_splitCombo->setCurrentIndex(passage.oneVersePerSlide ? 1 : 0);
    }

    QItemSelection selection;
    int first = -1;
    if (!passage.wholeChapter) {
        for (int i = 0; i < m_verseText->count(); ++i) {
            if (passage.verses.contains(m_verseText->item(i)->data(kVerseRole).toInt())) {
                const QModelIndex idx = m_verseText->model()->index(i, 0);
                selection.select(idx, idx);
                if (first < 0) first = i;
            }
        }
    }
    m_verseText->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    if (first >= 0) {
        m_verseText->scrollToItem(m_verseText->item(first), QAbstractItemView::PositionAtCenter);
    }
    updateSummary();
}

void BiblePage::startNew()
{
    m_verseText->clearSelection();
    const QSignalBlocker blocker(m_splitCombo);
    m_splitCombo->setCurrentIndex(0);
    updateSummary();
}

// ---------------------------------------------------------------------------
// Summary and preview
// ---------------------------------------------------------------------------

void BiblePage::updateSummary()
{
    const BiblePassage p = passage();
    if (!p.isValid()) {
        m_summary->setText(m_bible ? tr("Choose a book and a chapter.") : QString());
        m_previewList->clear();
        emit selectionChanged();
        return;
    }

    const int slides = SlideDeck::createBible(p)->count();
    QString text = p.reference();
    if (!p.abbreviation.isEmpty()) {
        text += QStringLiteral(" (%1)").arg(p.abbreviation);
    }
    text += QStringLiteral("  ·  ") + tr("%n verse(s)", "", int(p.verses.size()));
    if (p.wholeChapter) {
        text += QStringLiteral("  ·  ") + tr("whole chapter");
    }
    text += QStringLiteral("  ·  ") + tr("%n slide(s)", "", slides);
    m_summary->setText(text);

    m_previewTimer.start();
    emit selectionChanged();
}

void BiblePage::updatePreview()
{
    m_previewList->clear();
    const BiblePassage p = passage();
    if (!p.isValid()) {
        return;
    }
    const auto deck = SlideDeck::createBible(p);
    const qreal dpr = devicePixelRatioF();
    for (int i = 0; i < deck->count(); ++i) {
        QImage img = deck->render(i, kPreviewSize * dpr);
        img.setDevicePixelRatio(dpr);
        m_previewList->addItem(new QListWidgetItem(QIcon(QPixmap::fromImage(img)), QString::number(i + 1)));
    }
}

// ---------------------------------------------------------------------------
// Tile lists: height follows the content (up to maxRows rows, then scroll)
// ---------------------------------------------------------------------------

void BiblePage::fitTileList(QListWidget *list)
{
    const int maxRows = list->property("maxRows").toInt();
    const QSize grid = list->gridSize();
    const int width = list->contentsRect().width() - list->verticalScrollBar()->sizeHint().width();
    const int perRow = qMax(1, width / grid.width());
    const int rows = qMax(1, int((list->count() + perRow - 1) / perRow));
    list->setFixedHeight(qMin(rows, maxRows) * grid.height() + 2 * list->frameWidth() + 4);
}

bool BiblePage::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize && (obj == m_chapterList || obj == m_verseTiles)) {
        fitTileList(static_cast<QListWidget *>(obj));
    }
    return QWidget::eventFilter(obj, event);
}
