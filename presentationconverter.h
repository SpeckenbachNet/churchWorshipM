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

#include <QObject>
#include <QSet>
#include <QString>
#include <QThreadPool>
#include <atomic>

// Converts presentations (ppt, pptx, odp) to PDF in the background.
//
// The external office programs are tried in a fixed order per platform:
//   Windows: PowerPoint -> OnlyOffice -> LibreOffice
//   macOS:   PowerPoint -> Keynote -> OnlyOffice -> LibreOffice
//   Linux:   OnlyOffice -> LibreOffice
//
// Results are cached (key = path + modification time), so every file is converted only once.
// Jobs run one after another in a single worker thread: office programs don't like
// parallel automation, and LibreOffice shares one profile.
class PresentationConverter : public QObject {
    Q_OBJECT

public:
    explicit PresentationConverter(QObject *parent = nullptr);
    ~PresentationConverter() override;

    // Path of the converted PDF, or empty if not converted yet
    static QString cachedPdf(const QString &source);

    // Queues a conversion. Does nothing if already cached or already queued.
    void convert(const QString &source);

    bool isPending(const QString &source) const { return m_pending.contains(source); }

signals:
    // error is empty on success. Always emitted in the GUI thread.
    void finished(const QString &source, const QString &error);

private:
    QThreadPool      m_pool;
    QSet<QString>    m_pending;
    std::atomic_bool m_cancel{false};   // set on shutdown, running office processes get killed
};
