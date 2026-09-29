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
#include <QWidget>

class QScreen;
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

    // YouTube: the video is loaded paused and controlled from the main window
    void loadYouTube(const QString &videoId);
    void playVideo();
    void pauseVideo();
    void stopVideo();     // back to the start, paused
    void unloadVideo();   // removes the player, back to image mode

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
    void updateWebViewVisibility();

    QImage m_image;
    bool   m_black = false;

    QWebEngineView *m_webView = nullptr;   // created on first use (Chromium is heavy)
    bool m_videoMode = false;
};
