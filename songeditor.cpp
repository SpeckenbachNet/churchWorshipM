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
#include "songeditor.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace {

constexpr int kIdRole = Qt::UserRole + 1;
constexpr int kKindRole = Qt::UserRole + 2;

// Order entries as colored "chips": V1  C  V2  C  B  C
class ChipDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QFont font = option.font;
        font.setBold(true);
        const int w = QFontMetrics(font).horizontalAdvance(index.data().toString());
        return {qMax(40, w + 24), QFontMetrics(font).height() + 14};
    }

    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(option.rect).adjusted(2.5, 2.5, -2.5, -2.5);
        QColor color = SongPart::color(SongPart::Kind(index.data(kKindRole).toInt()));
        QPainterPath path;
        path.addRoundedRect(r, r.height() / 2, r.height() / 2);
        p->fillPath(path, color);
        if (option.state & QStyle::State_Selected) {
            p->setPen(QPen(option.palette.color(QPalette::HighlightedText), 2));
            p->drawPath(path);
        }
        QFont font = option.font;
        font.setBold(true);
        p->setFont(font);
        p->setPen(Qt::white);
        p->drawText(r, Qt::AlignCenter, index.data().toString());
        p->restore();
    }
};

} // namespace

SongEditorDialog::SongEditorDialog(const Song &song, const QStringList &order, Mode mode, QWidget *parent)
    : QDialog(parent), m_song(song), m_mode(mode)
{
    setWindowTitle(song.title.isEmpty() ? tr("New song") : tr("Edit song"));
    setStyleSheet(QStringLiteral("QLineEdit { padding: 4px 6px; }"));

    // --- Song data
    m_title = new QLineEdit(song.title, this);
    m_authors = new QLineEdit(song.authors, this);
    m_copyright = new QLineEdit(song.copyright, this);
    m_copyright->setPlaceholderText(tr("e.g. 2011 Thankyou Music"));
    m_ccli = new QLineEdit(song.ccliNumber, this);
    m_ccli->setPlaceholderText(tr("empty for own songs"));

    auto *data = new QFormLayout;
    data->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    data->setVerticalSpacing(10);
    data->addRow(tr("Title:"), m_title);
    data->addRow(tr("Authors:"), m_authors);
    auto *rights = new QHBoxLayout;
    rights->addWidget(m_copyright, 3);
    rights->addWidget(new QLabel(tr("CCLI no.:"), this));
    rights->addWidget(m_ccli, 1);
    data->addRow(tr("Copyright:"), rights);

    // --- Parts (left) and text of the selected part (right)
    m_parts = new QListWidget(this);
    m_parts->setMinimumWidth(170);
    m_addPartBtn = new QPushButton(tr("Add"), this);
    auto *addMenu = new QMenu(m_addPartBtn);
    for (SongPart::Kind kind : {SongPart::Verse, SongPart::PreChorus, SongPart::Chorus, SongPart::Bridge,
                                SongPart::Tag, SongPart::Intro, SongPart::Interlude, SongPart::Ending}) {
        SongPart sample;
        sample.kind = kind;
        addMenu->addAction(sample.label(), this, [this, kind] { addPart(kind); });
    }
    m_addPartBtn->setMenu(addMenu);
    m_removePartBtn = new QPushButton(tr("Remove"), this);
    m_toOrderBtn = new QPushButton(tr("Add to order"), this);
    m_toOrderBtn->setToolTip(tr("Appends the part to the order (also: double click)"));

    auto *partButtons = new QHBoxLayout;
    partButtons->addWidget(m_addPartBtn);
    partButtons->addWidget(m_removePartBtn);
    partButtons->addStretch();
    auto *partsBox = new QWidget(this);
    auto *partsLayout = new QVBoxLayout(partsBox);
    partsLayout->setContentsMargins(0, 0, 0, 0);
    partsLayout->addWidget(new QLabel(tr("Parts"), this));
    partsLayout->addWidget(m_parts, 1);
    partsLayout->addLayout(partButtons);
    partsLayout->addWidget(m_toOrderBtn);

    m_text = new QPlainTextEdit(this);
    m_text->document()->setDocumentMargin(8);
    auto *hint = new QLabel(tr("A line with --- starts a new slide within the part."), this);
    hint->setEnabled(false);   // muted
    auto *textBox = new QWidget(this);
    auto *textLayout = new QVBoxLayout(textBox);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->addWidget(new QLabel(tr("Lyrics of the part"), this));
    textLayout->addWidget(m_text, 1);
    textLayout->addWidget(hint);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(partsBox);
    splitter->addWidget(textBox);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);

    // --- Order: chips, drag to reorder, Del removes
    m_order = new QListWidget(this);
    m_order->setFlow(QListView::LeftToRight);
    m_order->setWrapping(true);
    m_order->setResizeMode(QListView::Adjust);
    m_order->setSpacing(1);
    m_order->setDragDropMode(QAbstractItemView::InternalMove);
    m_order->setDefaultDropAction(Qt::MoveAction);
    m_order->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_order->setItemDelegate(new ChipDelegate(m_order));
    m_order->setFixedHeight(3 * (fontMetrics().height() + 14) + 12);
    m_order->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto *removeFromOrder = new QAction(tr("Remove from order"), m_order);
    removeFromOrder->setShortcuts({QKeySequence::Delete, Qt::Key_Backspace});
    removeFromOrder->setShortcutContext(Qt::WidgetShortcut);
    m_order->addAction(removeFromOrder);

    auto *resetBtn = new QPushButton(tr("Every part once"), this);
    resetBtn->setToolTip(tr("Resets the order: every part once in the listed sequence"));
    auto *orderHead = new QHBoxLayout;
    auto *orderLabel = new QLabel(tr("Order (drag to rearrange, Del removes)"), this);
    orderHead->addWidget(orderLabel);
    orderHead->addStretch();
    orderHead->addWidget(resetBtn);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    layout->addLayout(data);
    layout->addWidget(splitter, 1);
    layout->addLayout(orderHead);
    layout->addWidget(m_order);
    if (m_mode == Event) {
        m_asDefault = new QCheckBox(tr("Use this order as default for the song"), this);
        m_asDefault->setToolTip(tr("Otherwise the order only applies to this event."));
        auto *note = new QLabel(tr("Changes of the lyrics apply to every event."), this);
        note->setEnabled(false);
        layout->addWidget(m_asDefault);
        layout->addWidget(note);
    }
    layout->addWidget(buttons);
    resize(820, 680);

    // --- Content
    refreshPartList();
    const QStringList initial = !order.isEmpty() ? order : song.order;
    for (const QString &id : initial) {
        appendToOrder(id);
    }

    connect(m_parts, &QListWidget::currentRowChanged, this, [this](int row) {
        storePart();
        loadPart(row);
        updateButtons();
    });
    connect(m_parts, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        appendToOrder(item->data(kIdRole).toString());
    });
    connect(m_toOrderBtn, &QPushButton::clicked, this, [this] {
        if (QListWidgetItem *item = m_parts->currentItem()) {
            appendToOrder(item->data(kIdRole).toString());
        }
    });
    connect(m_removePartBtn, &QPushButton::clicked, this, &SongEditorDialog::removePart);
    connect(removeFromOrder, &QAction::triggered, this, [this] {
        qDeleteAll(m_order->selectedItems());
    });
    connect(resetBtn, &QPushButton::clicked, this, &SongEditorDialog::resetOrder);
    connect(m_title, &QLineEdit::textChanged, this, &SongEditorDialog::updateButtons);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { storePart(); accept(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);

    m_parts->setCurrentRow(m_parts->count() > 0 ? 0 : -1);
    loadPart(m_parts->currentRow());
    updateButtons();
}

// ---------------------------------------------------------------------------
// Parts
// ---------------------------------------------------------------------------

void SongEditorDialog::refreshPartList()
{
    const QSignalBlocker blocker(m_parts);
    m_parts->clear();
    for (const SongPart &part : std::as_const(m_song.parts)) {
        auto *item = new QListWidgetItem(QStringLiteral("%1   %2").arg(part.id, part.label()), m_parts);
        item->setData(kIdRole, part.id);
        QPixmap dot(12, 12);
        dot.fill(Qt::transparent);
        QPainter p(&dot);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(SongPart::color(part.kind));
        p.setPen(Qt::NoPen);
        p.drawEllipse(dot.rect());
        item->setIcon(QIcon(dot));
    }
}

void SongEditorDialog::loadPart(int row)
{
    m_currentPart = row;
    const QSignalBlocker blocker(m_text);
    m_text->setPlainText(row >= 0 && row < m_song.parts.size() ? m_song.parts.at(row).text : QString());
    m_text->setEnabled(row >= 0);
}

void SongEditorDialog::storePart()
{
    if (m_currentPart >= 0 && m_currentPart < m_song.parts.size()) {
        m_song.parts[m_currentPart].text = m_text->toPlainText().trimmed();
    }
}

void SongEditorDialog::addPart(SongPart::Kind kind)
{
    storePart();
    SongPart part;
    part.kind = kind;
    // Verses are numbered, the others only if they exist already (Chorus, Chorus 2, ...)
    int count = 0;
    for (const SongPart &p : std::as_const(m_song.parts)) {
        count += p.kind == kind ? 1 : 0;
    }
    part.number = (kind == SongPart::Verse || count > 0) ? count + 1 : 0;
    const QString base = SongPart::idPrefix(kind) + (part.number > 0 ? QString::number(part.number) : QString());
    part.id = base;
    for (int n = 2; m_song.partIndex(part.id) >= 0; ++n) {
        part.id = QStringLiteral("%1-%2").arg(base).arg(n);
    }
    m_song.parts << part;
    refreshPartList();
    m_currentPart = -1;
    m_parts->setCurrentRow(int(m_song.parts.size()) - 1);
    appendToOrder(part.id);
    m_text->setFocus();
}

void SongEditorDialog::removePart()
{
    const int row = m_parts->currentRow();
    if (row < 0) {
        return;
    }
    const QString id = m_song.parts.at(row).id;
    m_currentPart = -1;   // text of the removed part is not stored back
    m_song.parts.removeAt(row);
    for (int i = m_order->count() - 1; i >= 0; --i) {
        if (m_order->item(i)->data(kIdRole).toString() == id) {
            delete m_order->takeItem(i);
        }
    }
    refreshPartList();
    m_parts->setCurrentRow(qMin(row, m_parts->count() - 1));
    loadPart(m_parts->currentRow());
    updateButtons();
}

// ---------------------------------------------------------------------------
// Order
// ---------------------------------------------------------------------------

void SongEditorDialog::updateOrderItem(QListWidgetItem *item) const
{
    const int i = m_song.partIndex(item->data(kIdRole).toString());
    const SongPart part = m_song.parts.value(i);
    item->setText(part.id);
    item->setData(kKindRole, int(part.kind));
    item->setToolTip(part.label());
}

void SongEditorDialog::appendToOrder(const QString &partId)
{
    if (m_song.partIndex(partId) < 0) {
        return;
    }
    auto *item = new QListWidgetItem(m_order);
    item->setData(kIdRole, partId);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    updateOrderItem(item);
    m_order->scrollToItem(item);
}

void SongEditorDialog::resetOrder()
{
    m_order->clear();
    for (const SongPart &part : std::as_const(m_song.parts)) {
        appendToOrder(part.id);
    }
}

void SongEditorDialog::updateButtons()
{
    const bool hasPart = m_parts->currentRow() >= 0;
    m_removePartBtn->setEnabled(hasPart);
    m_toOrderBtn->setEnabled(hasPart);
    if (m_okBtn) {
        m_okBtn->setEnabled(!m_title->text().trimmed().isEmpty());
    }
}

// ---------------------------------------------------------------------------
// Result
// ---------------------------------------------------------------------------

QStringList SongEditorDialog::order() const
{
    QStringList ids;
    for (int i = 0; i < m_order->count(); ++i) {
        ids << m_order->item(i)->data(kIdRole).toString();
    }
    return ids;
}

Song SongEditorDialog::song() const
{
    Song s = m_song;
    s.title = m_title->text().trimmed();
    s.authors = m_authors->text().trimmed();
    s.copyright = m_copyright->text().trimmed();
    if (s.copyright.startsWith(QChar(0x00A9))) {
        s.copyright = s.copyright.mid(1).trimmed();   // the (C) sign is added on the slides
    }
    s.ccliNumber = m_ccli->text().trimmed();
    s.order = order();
    return s;
}

bool SongEditorDialog::orderAsDefault() const
{
    return m_mode == Library || (m_asDefault && m_asDefault->isChecked());
}
