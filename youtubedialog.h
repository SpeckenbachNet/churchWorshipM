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

#include "mediaitem.h"

#include <QDialog>
#include <QPointer>
#include <QTimer>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPushButton;

// Adds a YouTube video or corrects its link. The link is checked while typing / pasting:
// title and preview image of the video, or a hint if it cannot be played.
class YouTubeDialog : public QDialog {
    Q_OBJECT

public:
    // 'url' empty: a YouTube link in the clipboard is taken over
    YouTubeDialog(QNetworkAccessManager *network, const QString &url, QWidget *parent = nullptr);

    QString url() const;
    QString title() const { return m_title; }   // empty if unknown
    MediaItem::YouTubeStatus status() const { return m_status; }

private:
    void check();
    void showResult(MediaItem::YouTubeStatus status, const QString &text);

    QNetworkAccessManager   *m_network;
    QPointer<QNetworkReply>  m_reply;        // running check
    QPointer<QNetworkReply>  m_thumbReply;
    QTimer                   m_checkTimer;   // checks shortly after the last key
    QString                  m_title;
    MediaItem::YouTubeStatus m_status = MediaItem::YouTubeUnchecked;

    QLineEdit   *m_url = nullptr;
    QLabel      *m_thumb = nullptr;
    QLabel      *m_titleLabel = nullptr;
    QLabel      *m_statusLabel = nullptr;
    QPushButton *m_okBtn = nullptr;
};
