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
#pragma once

#include "biblecatalog.h"

#include <QWidget>

class BibleInstaller;
class QLabel;
class QProgressBar;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

// Settings tab "Bibles": download from the catalog, import files, remove
class BibleSettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit BibleSettingsPage(QWidget *parent = nullptr);

    bool isBusy() const;

signals:
    void busyChanged(bool busy);

private:
    void refresh(const QString &selectId = {});
    void updateButtonStates();
    void downloadSelected();
    void importFromFile();
    void removeSelected();
    void onFinished(const QString &id, const QString &error);
    void setBusy(bool busy, const QString &status = {});

    QString selectedId() const;
    int     selectedCatalogIndex() const;   // -1 = imported bible (not from the catalog)

    QList<BibleCatalogEntry> m_catalog;
    BibleInstaller *m_installer = nullptr;

    QTreeWidget  *m_list = nullptr;
    QLabel       *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QToolButton  *m_downloadBtn = nullptr;
    QToolButton  *m_importBtn = nullptr;
    QToolButton  *m_removeBtn = nullptr;
};
