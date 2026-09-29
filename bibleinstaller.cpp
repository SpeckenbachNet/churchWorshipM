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
#include "bibleinstaller.h"
#include "bibleimporter.h"

#include <QCoreApplication>
#include <QNetworkReply>
#include <QUrl>

BibleInstaller::BibleInstaller(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
}

BibleInstaller::~BibleInstaller()
{
    if (m_reply) {
        m_reply->disconnect(this);   // no finished handling while we are being destroyed
        m_reply->abort();
        m_reply->deleteLater();
    }
    m_pool.waitForDone();            // a running import takes well under a second
}

void BibleInstaller::fail(const QString &error)
{
    m_file.close();
    m_tmp.reset();
    m_busy = false;
    emit finished(QString(), error);
}

void BibleInstaller::install(const BibleCatalogEntry &entry)
{
    if (m_busy) {
        return;
    }
    m_busy = true;

    m_tmp = std::make_unique<QTemporaryDir>();
    const QUrl url(entry.url);
    // Keep the original file name: the importer detects the format by its suffix
    m_file.setFileName(m_tmp->filePath(url.fileName()));
    if (!m_tmp->isValid() || !m_file.open(QIODevice::WriteOnly)) {
        fail(QCoreApplication::translate("BibleInstaller", "Cannot create a temporary folder."));
        return;
    }

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1/%2").arg(QCoreApplication::applicationName(),
                                                  QCoreApplication::applicationVersion()));
    m_reply = m_network.get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        m_file.write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, &BibleInstaller::downloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, [this, entry] {
        QNetworkReply *reply = m_reply;
        reply->deleteLater();
        m_file.write(reply->readAll());
        m_file.close();

        if (reply->error() != QNetworkReply::NoError) {
            fail(QCoreApplication::translate("BibleInstaller", "Download failed: %1").arg(reply->errorString()));
            return;
        }
        startImport(m_file.fileName(), entry.hint(), entry.swordModule);
    });
}

void BibleInstaller::importFile(const QString &path, const QString &swordModule)
{
    if (m_busy) {
        return;
    }
    m_busy = true;
    BibleInfo hint;
    hint.source = path;
    startImport(path, hint, swordModule);
}

void BibleInstaller::startImport(const QString &path, const BibleInfo &hint, const QString &swordModule)
{
    emit importStarted();
    m_pool.start([this, path, hint, swordModule] {
        QString error;
        const QString id = BibleImporter::importFile(path, hint, swordModule, &error);

        // Back to the GUI thread
        QMetaObject::invokeMethod(this, [this, id, error] {
            m_tmp.reset();
            m_busy = false;
            emit finished(id, error);
        }, Qt::QueuedConnection);
    });
}
