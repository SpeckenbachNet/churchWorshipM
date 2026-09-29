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
#include "biblesettingspage.h"
#include "bibleinstaller.h"
#include "swordimporter.h"
#include "toolbarm.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QSettings>
#include <QSet>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
constexpr int kIdRole      = Qt::UserRole + 1;
constexpr int kCatalogRole = Qt::UserRole + 2;

enum Column { NameColumn, LicenseColumn, StatusColumn };
}

BibleSettingsPage::BibleSettingsPage(QWidget *parent)
    : QWidget(parent),
    m_catalog(BibleCatalog::entries()),
    m_installer(new BibleInstaller(this))
{
    auto *intro = new QLabel(tr("Bibles are stored on this computer and can be used without internet."), this);
    intro->setWordWrap(true);

    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setAlternatingRowColors(true);
    m_list->setHeaderLabels({tr("Translation"), tr("License"), tr("Status")});
    m_list->header()->setSectionResizeMode(NameColumn, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(LicenseColumn, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(StatusColumn, QHeaderView::ResizeToContents);
    // More air than the default table look. The header only gets more height,
    // a style sheet on it would replace the native header look.
    m_list->setStyleSheet(QStringLiteral("QTreeView::item { padding: 8px 10px; }"));
    m_list->header()->setMinimumHeight(34);

    m_status = new QLabel(this);
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(8);
    m_status->hide();
    m_progress->hide();

    auto *toolbar = new ToolbarM(this);
    toolbar->setMinimumHeight(40);
    toolbar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    m_downloadBtn = toolbar->addButton("downloadBibleBtn", tr("Download"), ":icons/download", true);
    m_importBtn   = toolbar->addButton("importBibleBtn", tr("Import file"), ":icons/open", true);
    toolbar->addSpacer();
    m_removeBtn   = toolbar->addButton("removeBibleBtn", tr("Remove"), ":icons/remove", true);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_status);
    layout->addWidget(m_progress);
    layout->addWidget(toolbar);

    connect(m_list, &QTreeWidget::currentItemChanged, this, &BibleSettingsPage::updateButtonStates);
    connect(m_downloadBtn, &QToolButton::clicked, this, &BibleSettingsPage::downloadSelected);
    connect(m_importBtn,   &QToolButton::clicked, this, &BibleSettingsPage::importFromFile);
    connect(m_removeBtn,   &QToolButton::clicked, this, &BibleSettingsPage::removeSelected);

    connect(m_installer, &BibleInstaller::downloadProgress, this, [this](qint64 received, qint64 total) {
        m_progress->setRange(0, total > 0 ? 1000 : 0);   // 0..0 = busy indicator
        if (total > 0) {
            m_progress->setValue(int(received * 1000 / total));
        }
    });
    connect(m_installer, &BibleInstaller::importStarted, this, [this] {
        m_status->setText(tr("Converting..."));
        m_progress->setRange(0, 0);
    });
    connect(m_installer, &BibleInstaller::finished, this, &BibleSettingsPage::onFinished);

    refresh();
}

bool BibleSettingsPage::isBusy() const
{
    return m_installer->isBusy();
}

void BibleSettingsPage::refresh(const QString &selectId)
{
    const QString keep = selectId.isEmpty() ? selectedId() : selectId;
    m_list->clear();

    const QList<BibleInfo> installed = BibleStore::installed();
    QSet<QString> installedIds;
    for (const BibleInfo &info : installed) {
        installedIds.insert(info.id);
    }

    // 1. Catalog
    QSet<QString> catalogIds;
    for (int i = 0; i < m_catalog.size(); ++i) {
        const BibleCatalogEntry &e = m_catalog.at(i);
        catalogIds.insert(e.id);
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(NameColumn, QStringLiteral("%1 (%2)").arg(e.name, e.abbreviation));
        item->setText(LicenseColumn, e.license);
        item->setText(StatusColumn, installedIds.contains(e.id) ? tr("Installed") : tr("Not installed"));
        item->setToolTip(LicenseColumn, e.note.isEmpty() ? e.license : e.license + "\n" + e.note);
        item->setToolTip(NameColumn, e.note);
        item->setData(0, kIdRole, e.id);
        item->setData(0, kCatalogRole, i);
    }

    // 2. Own imports
    for (const BibleInfo &info : installed) {
        if (catalogIds.contains(info.id)) {
            continue;
        }
        auto *item = new QTreeWidgetItem(m_list);
        item->setText(NameColumn, info.abbreviation.isEmpty()
                                      ? info.name : QStringLiteral("%1 (%2)").arg(info.name, info.abbreviation));
        item->setText(LicenseColumn, info.license);
        item->setText(StatusColumn, tr("Imported"));
        item->setToolTip(LicenseColumn, info.license);
        item->setData(0, kIdRole, info.id);
        item->setData(0, kCatalogRole, -1);
    }

    for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
        if (m_list->topLevelItem(i)->data(0, kIdRole).toString() == keep) {
            m_list->setCurrentItem(m_list->topLevelItem(i));
        }
    }
    updateButtonStates();
}

QString BibleSettingsPage::selectedId() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(0, kIdRole).toString() : QString();
}

int BibleSettingsPage::selectedCatalogIndex() const
{
    const QTreeWidgetItem *item = m_list->currentItem();
    return item ? item->data(0, kCatalogRole).toInt() : -1;
}

void BibleSettingsPage::updateButtonStates()
{
    const bool busy = isBusy();
    const QString id = selectedId();
    const bool installed = !id.isEmpty() && QFileInfo::exists(BibleStore::pathFor(id));

    m_downloadBtn->setEnabled(!busy && selectedCatalogIndex() >= 0 && !installed);
    m_importBtn->setEnabled(!busy);
    m_removeBtn->setEnabled(!busy && installed);
}

void BibleSettingsPage::setBusy(bool busy, const QString &status)
{
    m_status->setText(status);
    m_status->setVisible(busy);
    m_progress->setVisible(busy);
    m_progress->setRange(0, 0);
    m_list->setEnabled(!busy);
    updateButtonStates();
    emit busyChanged(busy);
}

void BibleSettingsPage::downloadSelected()
{
    const int index = selectedCatalogIndex();
    if (index < 0) {
        return;
    }
    const BibleCatalogEntry &entry = m_catalog.at(index);

    if (entry.needsConsent) {
        const auto answer = QMessageBox::question(
            this, tr("Download bible"),
            tr("%1 is protected by copyright:\n\n%2\n\n"
               "Please make sure your church may use this translation (e.g. for projection "
               "in services). Download now?").arg(entry.name, entry.license));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    m_installer->install(entry);
    setBusy(true, tr("Downloading %1...").arg(entry.name));
}

void BibleSettingsPage::importFromFile()
{
    QSettings settings;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import bible"), settings.value("lastBibleDir").toString(),
        tr("Bibles (SWORD, OSIS, Zefania, getBible)") + " (*.zip *.xml *.json);;"
        + tr("All files") + " (*)");
    if (path.isEmpty()) {
        return;
    }
    settings.setValue("lastBibleDir", QFileInfo(path).absolutePath());

    // A SWORD ZIP may contain several modules -> let the user choose
    QString module;
    if (path.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive)) {
        const QStringList modules = SwordImporter::modulesInZip(path);
        if (modules.size() > 1) {
            bool ok = false;
            module = QInputDialog::getItem(this, tr("Import bible"),
                                           tr("The file contains several bibles. Which one?"),
                                           modules, 0, false, &ok);
            if (!ok) {
                return;
            }
        }
    }

    m_installer->importFile(path, module);
    setBusy(true, tr("Converting..."));
}

void BibleSettingsPage::removeSelected()
{
    const QTreeWidgetItem *item = m_list->currentItem();
    if (!item) {
        return;
    }
    const auto answer = QMessageBox::question(
        this, tr("Remove bible"), tr("Remove %1 from this computer?").arg(item->text(NameColumn)));
    if (answer != QMessageBox::Yes) {
        return;
    }
    if (!BibleStore::remove(selectedId())) {
        QMessageBox::warning(this, tr("Remove bible"), tr("The bible could not be removed."));
    }
    refresh();
}

void BibleSettingsPage::onFinished(const QString &id, const QString &error)
{
    setBusy(false);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Bible"), error);
    }
    refresh(id);
}
