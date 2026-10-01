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
#include <QVideoFrame>
#include <QWidget>

class QAudioOutput;
class QMediaPlayer;
class QScreen;
class QVideoWidget;
class QWebEngineView;

// Output window for the projector. Shows exactly one image, nothing else.
// It never takes the keyboard focus, so all control stays in the main window.
class BeamerWindow : public QWidget {
    Q_OBJECT

public:
    explicit BeamerWindow(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void setBlack(bool black);
    bool isBlack() const { return m_black; }

    // Videos (YouTube or a local file) are loaded paused and controlled from the main window
    enum class VideoEnd { Black, LastFrame, Loop };   // local videos: what happens at the end
    void loadYouTube(const QString &videoId);
    void loadVideo(const QString &path, VideoEnd end);
    void playVideo();
    void pauseVideo();
    void stopVideo();     // back to the start, paused
    void seekVideo(qint64 ms);   // local videos only
    void unloadVideo();   // removes the player, back to image mode

signals:
    // Local videos: for the position bar in the main window
    void videoPositionChanged(qint64 ms);
    void videoDurationChanged(qint64 ms);

public:

    // Fullscreen on the given screen, or a normal window if screen == nullptr
    void showOn(QScreen *screen);

    // Size in device pixels the slides should be rendered in
    QSize outputSize() const;

    // First screen that is not the primary one (nullptr if there is only one)
    static QScreen *projectorScreen();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateVideoVisibility();
    void videoEnded();

    QImage m_image;
    bool   m_black = false;

    enum class VideoMode { None, YouTube, Local };
    VideoMode m_videoMode = VideoMode::None;

    QWebEngineView *m_webView = nullptr;   // created on first use (Chromium is heavy)

    QMediaPlayer *m_player = nullptr;      // local videos, created on first use
    QAudioOutput *m_audio = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    VideoEnd      m_videoEnd = VideoEnd::Black;
    bool          m_videoFinished = false; // ended: black or the last frame instead of the video
    QVideoFrame   m_lastFrame;
    QImage        m_endImage;
};
