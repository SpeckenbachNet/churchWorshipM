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

#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTemporaryDir>
#include <QThreadPool>
#include <memory>

class QNetworkReply;

// Downloads a catalog bible or imports a local file, both in the background.
// One job at a time.
class BibleInstaller : public QObject {
    Q_OBJECT

public:
    explicit BibleInstaller(QObject *parent = nullptr);
    ~BibleInstaller() override;

    bool isBusy() const { return m_busy; }

    void install(const BibleCatalogEntry &entry);
    void importFile(const QString &path, const QString &swordModule = {});

signals:
    void downloadProgress(qint64 received, qint64 total);   // total = -1 if unknown
    void importStarted();
    void finished(const QString &id, const QString &error);  // error empty on success

private:
    void startImport(const QString &path, const BibleInfo &hint, const QString &swordModule);
    void fail(const QString &error);

    QNetworkAccessManager          m_network;
    QPointer<QNetworkReply>        m_reply;
    std::unique_ptr<QTemporaryDir> m_tmp;
    QFile                          m_file;
    QThreadPool                    m_pool;
    bool                           m_busy = false;
};
