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
#include "youtubedialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

constexpr QSize kThumbSize{240, 135};

} // namespace

YouTubeDialog::YouTubeDialog(QNetworkAccessManager *network, const QString &url, QWidget *parent)
    : QDialog(parent), m_network(network)
{
    setWindowTitle(tr("YouTube video"));
    setStyleSheet(QStringLiteral("QLineEdit { padding: 4px 6px; }"));

    QString initial = url;
    if (initial.isEmpty()) {
        // Links are usually copied from the browser
        const QString clipboard = QGuiApplication::clipboard()->text().trimmed();
        if (!MediaItem::youTubeId(clipboard).isEmpty()) {
            initial = clipboard;
        }
    }
    m_url = new QLineEdit(initial, this);

    m_thumb = new QLabel(this);
    m_thumb->setFixedSize(kThumbSize);
    m_thumb->setAlignment(Qt::AlignCenter);
    m_thumb->setStyleSheet(QStringLiteral("QLabel { background: palette(base); border-radius: 6px; }"));
    m_titleLabel = new QLabel(this);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setTextFormat(Qt::PlainText);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setTextFormat(Qt::PlainText);

    auto *info = new QVBoxLayout;
    info->setSpacing(6);
    info->addWidget(m_titleLabel);
    info->addWidget(m_statusLabel);
    info->addStretch();
    auto *result = new QHBoxLayout;
    result->setSpacing(14);
    result->addWidget(m_thumb, 0, Qt::AlignTop);
    result->addLayout(info, 1);

    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->addRow(tr("Link:"), m_url);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okBtn = buttons->button(QDialogButtonBox::Ok);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(16);
    layout->addLayout(form);
    layout->addLayout(result);
    layout->addWidget(buttons);
    resize(620, sizeHint().height());

    m_checkTimer.setSingleShot(true);
    m_checkTimer.setInterval(400);
    connect(&m_checkTimer, &QTimer::timeout, this, &YouTubeDialog::check);
    connect(m_url, &QLineEdit::textChanged, this, [this] { m_checkTimer.start(); });

    check();
    m_url->setFocus();
    m_url->selectAll();
}

QString YouTubeDialog::url() const
{
    return m_url->text().trimmed();
}

void YouTubeDialog::showResult(MediaItem::YouTubeStatus status, const QString &text)
{
    m_status = status;
    const bool problem = status == MediaItem::YouTubeNotFound || status == MediaItem::YouTubeNotEmbeddable;
    m_statusLabel->setStyleSheet(problem ? QStringLiteral("QLabel { color: #dc5046; }") : QString());
    m_statusLabel->setEnabled(problem);   // other notes muted
    m_statusLabel->setText(text);
}

void YouTubeDialog::check()
{
    if (m_reply) {
        m_reply->abort();
    }
    if (m_thumbReply) {
        m_thumbReply->abort();
    }
    m_title.clear();
    m_titleLabel->clear();
    m_thumb->clear();

    const QString link = url();
    const bool valid = !MediaItem::youTubeId(link).isEmpty();
    m_okBtn->setEnabled(valid);
    if (!valid) {
        showResult(MediaItem::YouTubeUnchecked,
                   link.isEmpty() ? QString() : tr("This is not a valid YouTube link."));
        if (!link.isEmpty()) {
            m_statusLabel->setStyleSheet(QStringLiteral("QLabel { color: #dc5046; }"));
            m_statusLabel->setEnabled(true);
        }
        return;
    }

    showResult(MediaItem::YouTubeUnchecked, tr("Checking..."));
    QNetworkReply *reply = m_network->get(QNetworkRequest(MediaItem::youTubeCheckUrl(link)));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return;   // replaced by a newer check
        }
        const QVariant httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
        if (!httpStatus.isValid()) {
            showResult(MediaItem::YouTubeUnchecked, tr("Cannot be checked (no internet)."));
            return;
        }
        const auto status = MediaItem::youTubeStatusFromHttp(httpStatus.toInt());
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        switch (status) {
        case MediaItem::YouTubeOk:
            m_title = o.value("title").toString();
            m_titleLabel->setText(m_title);
            showResult(status, o.value("author_name").toString());
            break;
        case MediaItem::YouTubeNotFound:
            showResult(status, tr("This video was not found on YouTube. Please check the link."));
            return;
        case MediaItem::YouTubeNotEmbeddable:
            showResult(status, tr("This video cannot be played here: it is private, or its owner "
                                  "allows no playback in other programs."));
            return;
        default:
            showResult(status, tr("Cannot be checked right now."));
            return;
        }
        // Preview image of the video
        const QUrl thumbUrl(o.value("thumbnail_url").toString());
        if (!thumbUrl.isValid()) {
            return;
        }
        QNetworkReply *thumbReply = m_network->get(QNetworkRequest(thumbUrl));
        m_thumbReply = thumbReply;
        connect(thumbReply, &QNetworkReply::finished, this, [this, thumbReply] {
            thumbReply->deleteLater();
            const QImage image = QImage::fromData(thumbReply->readAll());
            if (image.isNull()) {
                return;
            }
            const qreal dpr = devicePixelRatioF();
            // YouTube thumbnails are 4:3 with black bars: cut to 16:9
            QImage cropped = image;
            if (image.height() * 16 > image.width() * 9 + 8) {
                const int h = image.width() * 9 / 16;
                cropped = image.copy(0, (image.height() - h) / 2, image.width(), h);
            }
            QPixmap pm = QPixmap::fromImage(cropped.scaled(kThumbSize * dpr, Qt::KeepAspectRatio,
                                                           Qt::SmoothTransformation));
            pm.setDevicePixelRatio(dpr);
            m_thumb->setPixmap(pm);
        });
    });
}
