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
#include "backgroundpicker.h"
#include "medialibrary.h"

#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>

namespace {

MediaLibrary *s_library = nullptr;

constexpr QSize kPreviewSize{96, 54};
constexpr int kIdRole = Qt::UserRole + 1;

// Suggestions: strong or dark colors, white text stays readable on them
const struct { const char *name; const char *color; } kColors[] = {
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Night blue"), "#1e3a5f"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Petrol"),     "#0f4c5c"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Dark green"), "#1f4d3a"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Bordeaux"),   "#5c1f2e"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Violet"),     "#3d2a5c"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Brown"),      "#4a3426"},
    {QT_TRANSLATE_NOOP("BackgroundPicker", "Anthracite"), "#2d3036"},
};

bool libraryHasImages()
{
    if (s_library) {
        for (const LibraryEntry &e : s_library->entries()) {
            if (e.type == MediaItem::Image && !e.isMissing()) {
                return true;
            }
        }
    }
    return false;
}

QIcon colorIcon(const QColor &color)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(QColor(128, 128, 128));
    p.drawRoundedRect(QRectF(0.5, 0.5, 15, 15), 3, 3);
    return QIcon(pm);
}

// Image entries of the media library as a grid of thumbnails; returns the chosen id
QString pickLibraryImage(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(BackgroundPicker::tr("Background image"));
    auto *list = new QListWidget(&dlg);
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(160, 90));
    list->setGridSize(QSize(180, 130));
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setWordWrap(true);
    for (const LibraryEntry &e : s_library->entries()) {
        if (e.type != MediaItem::Image || e.isMissing()) {
            continue;
        }
        auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(s_library->thumbnail(e.id))), e.title, list);
        item->setData(kIdRole, e.id);
    }
    auto *hint = new QLabel(list->count() == 0
                                ? BackgroundPicker::tr("The media library has no images yet.")
                                : QString(), &dlg);
    hint->setEnabled(false);
    hint->setVisible(list->count() == 0);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    ok->setEnabled(false);
    QObject::connect(list, &QListWidget::currentItemChanged, ok, [ok](QListWidgetItem *item) {
        ok->setEnabled(item);
    });
    QObject::connect(list, &QListWidget::itemActivated, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(10);
    layout->addWidget(list, 1);
    layout->addWidget(hint);
    layout->addWidget(buttons);
    dlg.resize(640, 480);

    if (dlg.exec() != QDialog::Accepted || !list->currentItem()) {
        return {};
    }
    return list->currentItem()->data(kIdRole).toString();
}

} // namespace

void BackgroundPicker::setLibrary(MediaLibrary *library)
{
    s_library = library;
}

MediaLibrary *BackgroundPicker::library()
{
    return s_library;
}

SlideBackground BackgroundPicker::withCurrentPath(const SlideBackground &background)
{
    SlideBackground bg = background;
    if (bg.kind == SlideBackground::Image && s_library && !bg.libraryId.isEmpty()) {
        const LibraryEntry e = s_library->entry(bg.libraryId);
        if (e.isValid()) {
            bg.path = e.path();
        }
    }
    return bg;
}

BackgroundPicker::BackgroundPicker(const QString &inheritText, QWidget *parent)
    : QWidget(parent)
{
    const bool allowInherit = !inheritText.isEmpty();
    m_kind = new QComboBox(this);
    if (allowInherit) {
        m_kind->addItem(inheritText, int(SlideBackground::Inherit));
    }
    m_kind->addItem(tr("Black"), int(SlideBackground::Black));
    m_kind->addItem(tr("Color"), int(SlideBackground::Color));
    m_kind->addItem(tr("Image"), int(SlideBackground::Image));
    m_background.kind = allowInherit ? SlideBackground::Inherit : SlideBackground::Black;

    m_preview = new QLabel(this);
    m_preview->setFixedSize(kPreviewSize);

    // Color: suggestions and the system color dialog
    m_colorBtn = new QPushButton(tr("Color"), this);
    auto *colorMenu = new QMenu(m_colorBtn);
    for (const auto &c : kColors) {
        const QColor color(QLatin1String(c.color));
        colorMenu->addAction(colorIcon(color), tr(c.name), this, [this, color] { setColor(color); });
    }
    colorMenu->addSeparator();
    colorMenu->addAction(tr("More colors..."), this, [this] {
        const QColor color = QColorDialog::getColor(m_background.color.isValid() ? m_background.color
                                                                                 : QColor(kColors[0].color),
                                                    this, tr("Background color"));
        if (color.isValid()) {
            setColor(color);
        }
    });
    m_colorBtn->setMenu(colorMenu);

    // Image: from the media library or a file
    m_imageBtn = new QPushButton(tr("Choose image"), this);
    auto *imageMenu = new QMenu(m_imageBtn);
    imageMenu->addAction(tr("From the media library..."), this, [this] { chooseImage(true); });
    imageMenu->addAction(tr("File..."), this, [this] { chooseImage(false); });
    m_imageBtn->setMenu(imageMenu);

    // Darkening of the image
    m_dim = new QSlider(Qt::Horizontal, this);
    m_dim->setRange(0, 80);
    m_dim->setSingleStep(5);
    m_dim->setPageStep(10);
    m_dimValue = new QLabel(this);
    m_dimValue->setMinimumWidth(QFontMetrics(font()).horizontalAdvance(QStringLiteral("100 %")));
    m_dimRow = new QWidget(this);
    auto *dimLayout = new QHBoxLayout(m_dimRow);
    dimLayout->setContentsMargins(0, 0, 0, 0);
    dimLayout->addWidget(new QLabel(tr("Darken:"), m_dimRow));
    dimLayout->addWidget(m_dim, 1);
    dimLayout->addWidget(m_dimValue);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(m_kind);
    row->addWidget(m_colorBtn);
    row->addWidget(m_imageBtn);
    row->addStretch();

    auto *controls = new QVBoxLayout;
    controls->setContentsMargins(0, 0, 0, 0);
    controls->setSpacing(6);
    controls->addLayout(row);
    controls->addWidget(m_dimRow);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(m_preview, 0, Qt::AlignTop);
    layout->addLayout(controls, 1);

    connect(m_kind, &QComboBox::activated, this, &BackgroundPicker::kindChosen);
    connect(m_dim, &QSlider::valueChanged, this, [this](int value) {
        m_background.dim = value;
        updateControls();
        emit changed();
    });
    updateControls();
}

void BackgroundPicker::setBackground(const SlideBackground &background)
{
    m_background = background;
    if (m_kind->findData(int(m_background.kind)) < 0) {
        m_background.kind = SlideBackground::Black;   // the inherit choice is not offered here
    }
    m_image = QImage();
    if (m_background.kind == SlideBackground::Image) {
        m_image = QImage(withCurrentPath(m_background).path);
        if (!m_image.isNull()) {
            m_image = m_image.scaled(kPreviewSize * 4, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
    }
    updateControls();
}

SlideBackground BackgroundPicker::background() const
{
    return m_background;
}

void BackgroundPicker::setInherited(const SlideBackground &eventDefault)
{
    m_inherited = eventDefault;
    updateControls();
}

void BackgroundPicker::kindChosen(int index)
{
    const auto kind = SlideBackground::Kind(m_kind->itemData(index).toInt());
    if (kind == m_background.kind) {
        return;
    }
    if (kind == SlideBackground::Image && m_background.path.isEmpty() && m_background.libraryId.isEmpty()) {
        // No image yet: choose one right away (from the library if it has images),
        // the old choice stays if cancelled
        if (!chooseImage(libraryHasImages())) {
            updateControls();
        }
        return;
    }
    m_background.kind = kind;
    if (kind == SlideBackground::Color && !m_background.color.isValid()) {
        m_background.color = QColor(QLatin1String(kColors[0].color));
    }
    updateControls();
    emit changed();
}

bool BackgroundPicker::chooseImage(bool fromLibrary)
{
    if (!s_library) {
        return false;
    }
    QString id;
    if (fromLibrary) {
        id = pickLibraryImage(this);
    } else {
        QSettings settings;
        const QString path = QFileDialog::getOpenFileName(this, tr("Background image"),
                                                          settings.value("lastBackgroundDir").toString(),
                                                          MediaItem::imageFileFilter());
        if (path.isEmpty()) {
            return false;
        }
        settings.setValue("lastBackgroundDir", QFileInfo(path).absolutePath());
        // Copied into the library: the event keeps working when the original is moved
        QString error;
        id = s_library->addFile(path, false, &error);
        if (id.isEmpty()) {
            QMessageBox::warning(this, tr("Background image"), error);
            return false;
        }
    }
    const LibraryEntry e = s_library->entry(id);
    if (!e.isValid()) {
        return false;
    }
    SlideBackground bg = m_background;
    bg.kind = SlideBackground::Image;
    bg.libraryId = e.id;
    bg.path = e.path();
    setBackground(bg);
    emit changed();
    return true;
}

void BackgroundPicker::setColor(const QColor &color)
{
    m_background.kind = SlideBackground::Color;
    m_background.color = color;
    updateControls();
    emit changed();
}

void BackgroundPicker::updateControls()
{
    const SlideBackground::Kind kind = m_background.kind;
    m_kind->setCurrentIndex(m_kind->findData(int(kind)));
    m_colorBtn->setVisible(kind == SlideBackground::Color);
    m_colorBtn->setIcon(colorIcon(m_background.color));
    m_imageBtn->setVisible(kind == SlideBackground::Image);
    m_dimRow->setVisible(kind == SlideBackground::Image);
    {
        const QSignalBlocker blocker(m_dim);
        m_dim->setValue(m_background.dim);
    }
    m_dimValue->setText(QStringLiteral("%1 %").arg(m_background.dim));

    // Preview: a small slide with a word on it
    SlideBackground shown = m_background.resolved(m_inherited);
    QImage image = m_image;
    if (m_background.isInherit() && shown.kind == SlideBackground::Image) {
        image = QImage(withCurrentPath(shown).path);
        if (!image.isNull()) {
            image = image.scaled(kPreviewSize * 4, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        }
    }
    const qreal dpr = devicePixelRatioF();
    QImage preview = SlideBackground::compose(kPreviewSize * dpr, shown, image, [&](QPainter &p) {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(int(15 * dpr));
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRect(QPoint(0, 0), kPreviewSize * dpr), Qt::AlignCenter, tr("Text"));
    });
    preview.setDevicePixelRatio(dpr);
    m_preview->setPixmap(QPixmap::fromImage(preview));
}
