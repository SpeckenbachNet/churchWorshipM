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

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
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
    updateWebViewVisibility();
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
    m_videoMode = true;
    updateWebViewVisibility();
}

void BeamerWindow::playVideo()
{
    if (m_videoMode) {
        m_webView->page()->runJavaScript("player && player.playVideo();");
    }
}

void BeamerWindow::pauseVideo()
{
    if (m_videoMode) {
        m_webView->page()->runJavaScript("player && player.pauseVideo();");
    }
}

void BeamerWindow::stopVideo()
{
    if (m_videoMode) {
        m_webView->page()->runJavaScript("if (player) { player.pauseVideo(); player.seekTo(0, true); }");
    }
}

void BeamerWindow::unloadVideo()
{
    if (!m_videoMode) {
        return;
    }
    m_videoMode = false;
    m_webView->setHtml(QString());   // stops playback and sound
    updateWebViewVisibility();
}

void BeamerWindow::updateWebViewVisibility()
{
    if (m_webView) {
        // "Black" only hides the picture, the video keeps running
        m_webView->setVisible(m_videoMode && !m_black);
    }
}

void BeamerWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_webView) {
        m_webView->setGeometry(rect());
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

    if (m_black || m_image.isNull()) {
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform);
    const QSize target = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect r(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
    p.drawImage(r, m_image);
}
