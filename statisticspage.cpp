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
#include "statisticspage.h"
#include "eventstore.h"
#include "song.h"
#include "songstore.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateEdit>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QSaveFile>
#include <QSet>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kKeyRole = Qt::UserRole + 1;

enum Period { Last3Months, Last6Months, Last12Months, ThisYear, LastYear, Custom };

} // namespace

StatisticsPage::StatisticsPage(QWidget *parent)
    : QWidget(parent)
{
    m_period = new QComboBox(this);
    m_period->addItem(tr("Last 3 months"), Last3Months);
    m_period->addItem(tr("Last 6 months"), Last6Months);
    m_period->addItem(tr("Last 12 months"), Last12Months);
    m_period->addItem(tr("This year"), ThisYear);
    m_period->addItem(tr("Last year"), LastYear);
    m_period->addItem(tr("Custom"), Custom);
    m_from = new QDateEdit(this);
    m_to = new QDateEdit(this);
    for (QDateEdit *edit : {m_from, m_to}) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat(QLocale().dateFormat(QLocale::ShortFormat));
    }

    m_view = new QComboBox(this);
    m_view->addItem(tr("All songs"), AllSongs);
    m_view->addItem(tr("CCLI report"), CcliReport);
    m_notSung = new QCheckBox(tr("Not sung for a long time"), this);
    m_notSung->setToolTip(tr("Songs of the library that were not sung in the period"));

    auto *filters = new QHBoxLayout;
    filters->setSpacing(10);
    filters->addWidget(new QLabel(tr("Period:"), this));
    filters->addWidget(m_period);
    filters->addWidget(m_from);
    filters->addWidget(new QLabel(QStringLiteral("–"), this));
    filters->addWidget(m_to);
    filters->addSpacing(20);
    filters->addWidget(new QLabel(tr("View:"), this));
    filters->addWidget(m_view);
    filters->addWidget(m_notSung);
    filters->addStretch();

    m_table = new QTreeWidget(this);
    m_table->setRootIsDecorated(false);
    m_table->setUniformRowHeights(true);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    // Same look as the song library
    m_table->setStyleSheet(QStringLiteral("QTreeView::item { padding: 8px 10px; }"));
    m_table->header()->setMinimumHeight(34);

    // Events of the selected song
    m_usesTitle = new QLabel(this);
    m_usesTitle->setWordWrap(true);
    m_usesList = new QListWidget(this);
    auto *usesBox = new QWidget(this);
    auto *usesLayout = new QVBoxLayout(usesBox);
    usesLayout->setContentsMargins(0, 0, 0, 0);
    usesLayout->addWidget(m_usesTitle);
    usesLayout->addWidget(m_usesList, 1);

    auto *splitter = new QSplitter(this);
    splitter->addWidget(m_table);
    splitter->addWidget(usesBox);
    splitter->setStretchFactor(0, 1);
    splitter->setSizes({800, 280});
    splitter->setChildrenCollapsible(false);

    m_status = new QLabel(this);
    m_status->setEnabled(false);   // muted
    m_status->setWordWrap(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 6, 0, 0);
    layout->setSpacing(8);
    layout->addLayout(filters);
    layout->addWidget(splitter, 1);
    layout->addWidget(m_status);

    m_period->setCurrentIndex(m_period->findData(QSettings().value("statistics/period", Last6Months).toInt()));
    periodChosen();

    connect(m_period, &QComboBox::currentIndexChanged, this, [this] {
        QSettings().setValue("statistics/period", m_period->currentData());
        periodChosen();
        refresh();
    });
    connect(m_from, &QDateEdit::dateChanged, this, [this] {
        if (m_period->currentData().toInt() == Custom) {
            refresh();
        }
    });
    connect(m_to, &QDateEdit::dateChanged, this, [this] {
        if (m_period->currentData().toInt() == Custom) {
            refresh();
        }
    });
    connect(m_view, &QComboBox::currentIndexChanged, this, &StatisticsPage::updateTable);
    connect(m_notSung, &QCheckBox::toggled, this, &StatisticsPage::updateTable);
    connect(m_table, &QTreeWidget::currentItemChanged, this, &StatisticsPage::showUses);
}

void StatisticsPage::setStores(EventStore *events, SongStore *songs)
{
    m_events = events;
    m_songs = songs;
}

void StatisticsPage::setView(View view)
{
    m_view->setCurrentIndex(m_view->findData(view));
}

void StatisticsPage::periodChosen()
{
    const QDate today = QDate::currentDate();
    const int period = m_period->currentData().toInt();
    const QSignalBlocker blockFrom(m_from);
    const QSignalBlocker blockTo(m_to);
    switch (period) {
    case Last3Months:  m_from->setDate(today.addMonths(-3)); m_to->setDate(today); break;
    case Last6Months:  m_from->setDate(today.addMonths(-6)); m_to->setDate(today); break;
    case Last12Months: m_from->setDate(today.addMonths(-12)); m_to->setDate(today); break;
    case ThisYear:     m_from->setDate(QDate(today.year(), 1, 1)); m_to->setDate(today); break;
    case LastYear:     m_from->setDate(QDate(today.year() - 1, 1, 1));
                       m_to->setDate(QDate(today.year() - 1, 12, 31)); break;
    default:           break;
    }
    m_from->setEnabled(period == Custom);
    m_to->setEnabled(period == Custom);
}

// ---------------------------------------------------------------------------
// Counting
// ---------------------------------------------------------------------------

void StatisticsPage::refresh()
{
    m_sung.clear();
    m_ccli.clear();
    if (!m_events || !m_songs) {
        return;
    }
    const QDate from = m_from->date();
    const QDate to = qMin(m_to->date(), QDate::currentDate());   // only events that have taken place

    // Newest first: the first use found is the last one
    for (const EventInfo &event : m_events->events()) {
        if (!event.date.isValid() || event.date > QDate::currentDate()) {
            continue;
        }
        const bool inPeriod = event.date >= from && event.date <= to;
        QSet<QString> sungHere;   // once per event
        QSet<QString> ccliHere;

        // A song as it is known now (library) or as copied into the event
        const auto describe = [this](const QJsonObject &o, const QString &fallbackTitle, QString *key) {
            Entry e;
            const QString id = o.value("id").toString();
            const Song song = m_songs->song(id);
            e.title = song.isValid() ? song.title : o.value("title").toString(fallbackTitle);
            e.language = song.isValid() ? song.language : QString();
            e.ccli = song.isValid() ? song.ccliNumber : o.value("ccli").toString();
            *key = song.isValid() ? id : QStringLiteral("title:") + e.title;
            return e;
        };
        const auto count = [&](QHash<QString, Entry> &map, QSet<QString> &here, const QString &key, const Entry &e) {
            if (here.contains(key)) {
                return;
            }
            here.insert(key);
            Entry &entry = map[key];
            if (entry.title.isEmpty()) {
                entry.title = e.title;
                entry.language = e.language;
                entry.ccli = e.ccli;
            }
            if (!entry.lastEver.isValid()) {
                entry.lastEver = event.date;
            }
            if (inPeriod) {
                entry.uses.append({event.date, event.name});
            }
        };

        for (const QJsonValue &v : m_events->items(event.id)) {
            const QJsonObject item = v.toObject();
            if (item.value("type").toString() != QLatin1String("song")) {
                continue;
            }
            const QJsonObject songObject = item.value("song").toObject();
            QString key;
            const Entry main = describe(songObject, item.value("title").toString(), &key);
            count(m_sung, sungHere, key, main);

            // CCLI: every song shown, each with its own number
            QList<Entry> shown{main};
            const QJsonObject translation = songObject.value("translation").toObject();
            if (!translation.value("slides").toArray().isEmpty()) {
                QString unused;
                shown << describe(translation, QString(), &unused);
            }
            for (const QJsonValue &other : songObject.value("others").toArray()) {
                QString unused;
                shown << describe(other.toObject(), QString(), &unused);
            }
            for (const Entry &e : std::as_const(shown)) {
                if (!e.ccli.isEmpty()) {
                    count(m_ccli, ccliHere, e.ccli, e);
                }
            }
        }
    }
    updateTable();
}

// ---------------------------------------------------------------------------
// Table
// ---------------------------------------------------------------------------

void StatisticsPage::updateTable()
{
    const QString keep = m_table->currentItem() ? m_table->currentItem()->data(0, kKeyRole).toString() : QString();
    const bool ccli = m_view->currentData().toInt() == CcliReport;
    const bool notSung = !ccli && m_notSung->isChecked();
    m_notSung->setVisible(!ccli);

    const QSignalBlocker blocker(m_table);
    m_table->clear();
    m_table->setSortingEnabled(false);
    enum { ColTitle, ColLanguage, ColCount, ColLast, ColCcli };
    QTreeWidgetItem *current = nullptr;
    int songs = 0;
    int total = 0;

    if (ccli) {
        m_table->setColumnCount(3);   // fewer columns than "All songs"
        m_table->setHeaderLabels({tr("CCLI no."), tr("Song"), tr("Uses")});
        for (auto it = m_ccli.cbegin(); it != m_ccli.cend(); ++it) {
            if (it->uses.isEmpty()) {
                continue;
            }
            auto *item = new QTreeWidgetItem(m_table);
            item->setData(0, kKeyRole, it.key());
            item->setData(0, Qt::DisplayRole, it->ccli.toLongLong());   // sorts as number
            item->setText(1, it->title);
            item->setData(2, Qt::DisplayRole, int(it->uses.size()));
            ++songs;
            total += int(it->uses.size());
            if (it.key() == keep) {
                current = item;
            }
        }
        m_table->sortByColumn(0, Qt::AscendingOrder);
        m_status->setText(tr("%n song(s) with CCLI number in the period, %1 uses in total. "
                             "Translations shown below and verses in another language count with "
                             "their own CCLI number.", "", songs).arg(total));
    } else {
        m_table->setColumnCount(5);
        m_table->setHeaderLabels({tr("Song"), tr("Language"), tr("Uses"), tr("Last sung"), tr("CCLI no.")});
        const auto addRow = [&](const QString &key, const Entry &e) {
            auto *item = new QTreeWidgetItem(m_table);
            item->setData(ColTitle, kKeyRole, key);
            item->setText(ColTitle, e.title);
            item->setText(ColLanguage, e.language.isEmpty() ? QString() : Song::languageName(e.language));
            item->setData(ColCount, Qt::DisplayRole, int(e.uses.size()));
            if (e.lastEver.isValid()) {
                item->setData(ColLast, Qt::DisplayRole, e.lastEver);
            } else {
                item->setText(ColLast, tr("never"));
            }
            item->setText(ColCcli, e.ccli);
            if (key == keep) {
                current = item;
            }
        };
        if (notSung) {
            // Library songs without a use in the period, longest ago first
            for (const Song &song : m_songs ? m_songs->songs() : QList<Song>()) {
                const Entry e = m_sung.value(song.id);
                if (!e.uses.isEmpty()) {
                    continue;
                }
                Entry row = e;
                row.title = song.title;
                row.language = song.language;
                row.ccli = song.ccliNumber;
                addRow(song.id, row);
                ++songs;
            }
            m_table->sortByColumn(ColLast, Qt::AscendingOrder);
            m_status->setText(tr("%n song(s) of the library were not sung in the period.", "", songs));
        } else {
            for (auto it = m_sung.cbegin(); it != m_sung.cend(); ++it) {
                if (it->uses.isEmpty()) {
                    continue;
                }
                addRow(it.key(), *it);
                ++songs;
                total += int(it->uses.size());
            }
            m_table->sortByColumn(ColCount, Qt::DescendingOrder);
            m_status->setText(tr("%n different song(s) in the period, sung %1 times in total "
                                 "(once per event).", "", songs).arg(total));
        }
    }
    m_table->setSortingEnabled(true);
    for (int c = 0; c < m_table->columnCount(); ++c) {
        m_table->resizeColumnToContents(c);
    }
    m_table->setColumnWidth(ccli ? 1 : ColTitle, qMax(260, m_table->columnWidth(ccli ? 1 : ColTitle)));
    m_table->setCurrentItem(current ? current : m_table->topLevelItem(0));
    showUses();
}

void StatisticsPage::showUses()
{
    m_usesList->clear();
    const QTreeWidgetItem *item = m_table->currentItem();
    if (!item) {
        m_usesTitle->clear();
        return;
    }
    const bool ccli = m_view->currentData().toInt() == CcliReport;
    const QString key = item->data(0, kKeyRole).toString();
    const Entry e = ccli ? m_ccli.value(key) : m_sung.value(key);
    m_usesTitle->setText(QStringLiteral("<b>%1</b>").arg(e.title.toHtmlEscaped()));
    for (const Use &use : e.uses) {
        m_usesList->addItem(QStringLiteral("%1  ·  %2")
                                .arg(QLocale().toString(use.date, QLocale::ShortFormat), use.eventName));
    }
    if (e.uses.isEmpty()) {
        m_usesList->addItem(e.lastEver.isValid()
                                ? tr("Last sung: %1").arg(QLocale().toString(e.lastEver, QLocale::ShortFormat))
                                : tr("Never sung"));
    }
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------

QStringList StatisticsPage::exportLines(QChar separator) const
{
    // Exactly what the table shows, in its order
    QStringList lines;
    QStringList header;
    for (int c = 0; c < m_table->columnCount(); ++c) {
        header << m_table->headerItem()->text(c);
    }
    lines << header.join(separator);
    for (int i = 0; i < m_table->topLevelItemCount(); ++i) {
        const QTreeWidgetItem *item = m_table->topLevelItem(i);
        QStringList cells;
        for (int c = 0; c < m_table->columnCount(); ++c) {
            QString cell = item->text(c);
            if (separator == QLatin1Char(';') && (cell.contains(separator) || cell.contains(QLatin1Char('"')))) {
                cell = QLatin1Char('"') + cell.replace(QLatin1Char('"'), QLatin1String("\"\"")) + QLatin1Char('"');
            }
            cells << cell;
        }
        lines << cells.join(separator);
    }
    return lines;
}

void StatisticsPage::copyToClipboard()
{
    // Tabs: pastes into a spreadsheet as columns
    QGuiApplication::clipboard()->setText(exportLines(QLatin1Char('\t')).join(QLatin1Char('\n')));
    m_status->setText(tr("The list is in the clipboard."));
}

void StatisticsPage::saveCsv()
{
    const bool ccli = m_view->currentData().toInt() == CcliReport;
    const QString name = QStringLiteral("%1 %2 - %3.csv")
                             .arg(ccli ? tr("CCLI report") : tr("Song statistics"),
                                  m_from->date().toString(Qt::ISODate), m_to->date().toString(Qt::ISODate));
    QSettings settings;
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Save as CSV"), settings.value("statistics/dir").toString() + QLatin1Char('/') + name,
        tr("CSV file (*.csv)"));
    if (path.isEmpty()) {
        return;
    }
    settings.setValue("statistics/dir", QFileInfo(path).absolutePath());
    // Semicolons and a byte order mark: opens correctly in Excel and Numbers
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write("\xEF\xBB\xBF");
        file.write(exportLines(QLatin1Char(';')).join(QLatin1String("\r\n")).toUtf8());
        file.write("\r\n");
    }
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Save as CSV"), tr("Cannot write %1.").arg(path));
    }
}
