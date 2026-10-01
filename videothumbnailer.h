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

#include <QImage>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

class QMediaPlayer;
class QVideoFrame;
class QVideoSink;

// Takes a frame of a video (about one second in) as preview image, in the background.
// Videos are handled one after another; nothing is played aloud.
class VideoThumbnailer : public QObject {
    Q_OBJECT

public:
    explicit VideoThumbnailer(QObject *parent = nullptr);

    void request(const QString &id, const QString &path);   // 'ready' follows

signals:
    void ready(const QString &id, const QImage &image);    // null image if it failed

private:
    void startNext();
    void frameArrived(const QVideoFrame &frame);
    void finish(const QImage &image);

    struct Job {
        QString id;
        QString path;
    };
    QList<Job>    m_queue;
    QString       m_currentId;   // empty = idle
    qint64        m_target = 0;  // position of the frame in ms
    QMediaPlayer *m_player = nullptr;
    QVideoSink   *m_sink = nullptr;
    QTimer        m_timeout;
};
