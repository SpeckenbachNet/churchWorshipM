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

#include "slidebackground.h"

#include <QWidget>

class MediaLibrary;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

// Choice of the background of text slides: (default), black, color or image, with a small preview.
// Images come from the media library; a file chosen from disk is added to the library first.
class BackgroundPicker : public QWidget {
    Q_OBJECT

public:
    // 'inheritText': name of the first choice that takes the background from elsewhere,
    // e.g. "Default" (entries) or "None" (songs); empty: no such choice (the event itself)
    explicit BackgroundPicker(const QString &inheritText, QWidget *parent = nullptr);

    // Set once by the main window: the pickers live in several dialogs
    static void          setLibrary(MediaLibrary *library);
    static MediaLibrary *library();
    // Current file of a library image ('path' is only a fallback)
    static SlideBackground withCurrentPath(const SlideBackground &background);

    void            setBackground(const SlideBackground &background);
    SlideBackground background() const;

    // Shown in the preview while the inherit choice is chosen
    void setInherited(const SlideBackground &eventDefault);

signals:
    void changed();

private:
    void kindChosen(int index);
    bool chooseImage(bool fromLibrary);
    void setColor(const QColor &color);
    void updateControls();

    SlideBackground m_background;
    SlideBackground m_inherited;
    QImage          m_image;    // small copy of the image for the preview

    QComboBox   *m_kind = nullptr;
    QPushButton *m_colorBtn = nullptr;
    QPushButton *m_imageBtn = nullptr;
    QLabel      *m_preview = nullptr;
    QWidget     *m_dimRow = nullptr;
    QSlider     *m_dim = nullptr;
    QLabel      *m_dimValue = nullptr;
};
