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
#include "beamerwindow.h"

#include <QAudioOutput>
#include <QGuiApplication>
#include <QMediaPlayer>
#include <QPainter>
#include <QScreen>
#include <QVideoSink>
#include <QVideoWidget>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QWindow>

namespace {

// YouTube IFrame API: gives us a 'player' object we can control via JavaScript.
// The page needs a real https origin, otherwise YouTube refuses to play (error 153).
const QUrl kYouTubeOrigin(QStringLiteral("https://localhost/"));

const char *kYouTubeHtml = R"(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<style>html,body{margin:0;height:100%;background:#000;overflow:hidden}#player{width:100%;height:100%}</style>
</head><body><div id="player"></div>
<script src="https://www.youtube.com/iframe_api"></script>
<script>
var player;
function onYouTubeIframeAPIReady() {
    player = new YT.Player('player', {
        videoId: '%1', width: '100%', height: '100%',
        playerVars: { controls: 0, rel: 0, playsinline: 1, iv_load_policy: 3,
                      origin: 'https://localhost' }
    });
}
</script></body></html>)";

} // namespace

BeamerWindow::BeamerWindow(QWidget *parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus)
{
    setWindowTitle(tr("Projector"));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setCursor(Qt::BlankCursor);
}

void BeamerWindow::setImage(const QImage &image)
{
    m_image = image;
    update();
}

void BeamerWindow::setBlack(bool black)
{
    m_black = black;
    updateVideoVisibility();
    update();
}

void BeamerWindow::loadYouTube(const QString &videoId)
{
    if (!m_webView) {
        m_webView = new QWebEngineView(this);
        m_webView->setFocusPolicy(Qt::NoFocus);
    }
    // A fresh page for every video: loaded a second time into the same page, the YouTube
    // API never creates the player. The old page is a child of the view, setPage() deletes it.
    m_webView->setPage(new QWebEnginePage(m_webView));
    // Allow play() from our JavaScript without a click into the page
    m_webView->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture, false);
    m_webView->setHtml(QString::fromLatin1(kYouTubeHtml).arg(videoId), kYouTubeOrigin);
    m_webView->setGeometry(rect());
    m_videoMode = VideoMode::YouTube;
    updateVideoVisibility();
}

void BeamerWindow::loadVideo(const QString &path, VideoEnd end)
{
    unloadVideo();
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_audio = new QAudioOutput(this);
        m_player->setAudioOutput(m_audio);
        m_videoWidget = new QVideoWidget(this);
        m_videoWidget->setFocusPolicy(Qt::NoFocus);
        m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
        m_videoWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_player->setVideoOutput(m_videoWidget);
        connect(m_player, &QMediaPlayer::positionChanged, this, &BeamerWindow::videoPositionChanged);
        connect(m_player, &QMediaPlayer::durationChanged, this, &BeamerWindow::videoDurationChanged);
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (status == QMediaPlayer::EndOfMedia && m_videoMode == VideoMode::Local) {
                videoEnded();
            }
        });
        // Remembered for "keep the last frame" (a frame is only a reference, no copy)
        connect(m_videoWidget->videoSink(), &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &frame) {
            if (m_videoEnd == VideoEnd::LastFrame && frame.isValid()) {
                m_lastFrame = frame;
            }
        });
    }
    m_videoEnd = end;
    m_videoFinished = false;
    m_lastFrame = QVideoFrame();
    m_endImage = QImage();
    m_player->setLoops(end == VideoEnd::Loop ? QMediaPlayer::Infinite : 1);
    m_player->setSource(QUrl::fromLocalFile(path));
    m_player->pause();   // shows the first frame
    m_videoWidget->setGeometry(rect());
    m_videoMode = VideoMode::Local;
    updateVideoVisibility();
}

void BeamerWindow::videoEnded()
{
    m_videoFinished = true;
    if (m_videoEnd == VideoEnd::LastFrame && m_lastFrame.isValid()) {
        m_endImage = m_lastFrame.toImage();
    }
    updateVideoVisibility();
    update();
}

void BeamerWindow::playVideo()
{
    if (m_videoMode == VideoMode::YouTube) {
        m_webView->page()->runJavaScript("player && player.playVideo();");
    } else if (m_videoMode == VideoMode::Local) {
        if (m_videoFinished) {
            m_videoFinished = false;   // again from the start
            m_endImage = QImage();
            m_player->setPosition(0);
            updateVideoVisibility();
            update();
        }
        m_player->play();
    }
}

void BeamerWindow::pauseVideo()
{
    if (m_videoMode == VideoMode::YouTube) {
        m_webView->page()->runJavaScript("player && player.pauseVideo();");
    } else if (m_videoMode == VideoMode::Local) {
        m_player->pause();
    }
}

void BeamerWindow::stopVideo()
{
    if (m_videoMode == VideoMode::YouTube) {
        m_webView->page()->runJavaScript("if (player) { player.pauseVideo(); player.seekTo(0, true); }");
    } else if (m_videoMode == VideoMode::Local) {
        m_videoFinished = false;
        m_endImage = QImage();
        m_player->pause();
        m_player->setPosition(0);
        updateVideoVisibility();
        update();
    }
}

void BeamerWindow::seekVideo(qint64 ms)
{
    if (m_videoMode == VideoMode::Local) {
        if (m_videoFinished) {
            m_videoFinished = false;
            m_endImage = QImage();
            updateVideoVisibility();
            update();
        }
        m_player->setPosition(ms);
    }
}

void BeamerWindow::unloadVideo()
{
    if (m_videoMode == VideoMode::YouTube) {
        m_webView->setHtml(QString());   // stops playback and sound
    } else if (m_videoMode == VideoMode::Local) {
        m_player->stop();
        m_player->setSource(QUrl());
        m_lastFrame = QVideoFrame();
        m_endImage = QImage();
        m_videoFinished = false;
    } else {
        return;
    }
    m_videoMode = VideoMode::None;
    updateVideoVisibility();
    update();
}

void BeamerWindow::updateVideoVisibility()
{
    // "Black" only hides the picture, the video keeps running
    if (m_webView) {
        m_webView->setVisible(m_videoMode == VideoMode::YouTube && !m_black);
    }
    if (m_videoWidget) {
        m_videoWidget->setVisible(m_videoMode == VideoMode::Local && !m_black && !m_videoFinished);
    }
}

void BeamerWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_webView) {
        m_webView->setGeometry(rect());
    }
    if (m_videoWidget) {
        m_videoWidget->setGeometry(rect());
    }
}

QScreen *BeamerWindow::projectorScreen()
{
    const QScreen *primary = QGuiApplication::primaryScreen();
    for (QScreen *screen : QGuiApplication::screens()) {
        if (screen != primary) {
            return screen;
        }
    }
    return nullptr;
}

void BeamerWindow::showOn(QScreen *screen)
{
    if (!screen) {
        // Only one screen: normal window for testing at home
        setWindowFlag(Qt::FramelessWindowHint, false);
        resize(960, 540);
        showNormal();
        return;
    }

    setWindowFlag(Qt::FramelessWindowHint, true);
    // The native window must exist before we can move it to another screen
    winId();
    windowHandle()->setScreen(screen);
    setGeometry(screen->geometry());
    showFullScreen();
}

QSize BeamerWindow::outputSize() const
{
    if (isFullScreen() && screen()) {
        return screen()->size() * screen()->devicePixelRatio();
    }
    return size() * devicePixelRatioF();
}

void BeamerWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    // A local video that has ended shows its last frame (or black) instead of the slide
    const QImage &image = m_videoMode == VideoMode::Local ? m_endImage : m_image;
    if (m_black || image.isNull()) {
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize target = image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect r(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
    p.drawImage(r, image);
}
