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
#include "videothumbnailer.h"

#include <QMediaPlayer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

VideoThumbnailer::VideoThumbnailer(QObject *parent)
    : QObject(parent)
{
    // No audio output: the player stays silent
    m_player = new QMediaPlayer(this);
    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(8000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finish(QImage()); });

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (m_currentId.isEmpty()) {
            return;
        }
        if (status == QMediaPlayer::LoadedMedia) {
            // About one second in: the very first frame is often black
            m_target = qMin<qint64>(1000, m_player->duration() / 10);
            m_player->setPosition(m_target);
            m_player->play();
        } else if (status == QMediaPlayer::InvalidMedia) {
            finish(QImage());
        }
    });
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &VideoThumbnailer::frameArrived);
}

void VideoThumbnailer::request(const QString &id, const QString &path)
{
    m_queue.append({id, path});
    if (m_currentId.isEmpty()) {
        startNext();
    }
}

void VideoThumbnailer::startNext()
{
    if (m_queue.isEmpty()) {
        return;
    }
    const Job job = m_queue.takeFirst();
    m_currentId = job.id;
    m_timeout.start();
    m_player->setSource(QUrl::fromLocalFile(job.path));
}

void VideoThumbnailer::frameArrived(const QVideoFrame &frame)
{
    if (m_currentId.isEmpty() || !frame.isValid() || m_player->playbackState() != QMediaPlayer::PlayingState) {
        return;
    }
    // startTime is in microseconds; frames before the target come from the start of decoding
    if (frame.startTime() >= 0 && frame.startTime() / 1000 + 100 < m_target) {
        return;
    }
    finish(frame.toImage());
}

void VideoThumbnailer::finish(const QImage &image)
{
    const QString id = m_currentId;
    m_currentId.clear();
    m_timeout.stop();
    m_player->stop();
    m_player->setSource(QUrl());
    emit ready(id, image);
    startNext();
}
