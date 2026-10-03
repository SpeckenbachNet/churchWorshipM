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
#include "settingspage.h"
#include "biblesettingspage.h"

#include <QCheckBox>
#include <QLabel>
#include <QRadioButton>
#include <QSettings>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

// Tab "Media library": what happens with files that are added
QWidget *createLibraryTab(QWidget *parent)
{
    auto *tab = new QWidget(parent);
    auto *intro = new QLabel(SettingsPage::tr("When files are added to the media library:"), tab);

    auto *copy = new QRadioButton(SettingsPage::tr("Copy into the media library (recommended)"), tab);
    auto *copyHint = new QLabel(SettingsPage::tr("The playlist keeps working even if the original is moved or "
                                            "the USB stick is missing. If the original changes, the media "
                                            "library offers to update the copy."), tab);
    auto *link = new QRadioButton(SettingsPage::tr("Link only – the file stays where it is"), tab);
    auto *linkHint = new QLabel(SettingsPage::tr("Useful for a shared cloud folder: the newest version is always "
                                            "shown automatically. The file has to be available during the service."),
                                tab);
    for (QLabel *hint : {copyHint, linkHint}) {
        hint->setWordWrap(true);
        hint->setEnabled(false);          // muted
        hint->setContentsMargins(24, 0, 0, 8);
    }

    const bool linkByDefault = QSettings().value("library/linkByDefault", false).toBool();
    (linkByDefault ? link : copy)->setChecked(true);
    QObject::connect(link, &QRadioButton::toggled, tab, [](bool checked) {
        QSettings().setValue("library/linkByDefault", checked);
    });

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(intro);
    layout->addWidget(copy);
    layout->addWidget(copyHint);
    layout->addWidget(link);
    layout->addWidget(linkHint);
    layout->addStretch(1);
    return tab;
}

QWidget *createSlidesTab(QWidget *parent)
{
    auto *tab = new QWidget(parent);
    auto *leadingBlank = new QCheckBox(SettingsPage::tr("Black slide at the beginning of songs, bible texts "
                                                        "and own slides"), tab);
    auto *hint = new QLabel(SettingsPage::tr("Nothing is shown on the projector when the entry is chosen; "
                                             "the text appears with the next click."), tab);
    hint->setWordWrap(true);
    hint->setEnabled(false);          // muted
    hint->setContentsMargins(24, 0, 0, 8);

    leadingBlank->setChecked(QSettings().value("slides/leadingBlank", true).toBool());
    QObject::connect(leadingBlank, &QCheckBox::toggled, tab, [](bool checked) {
        QSettings().setValue("slides/leadingBlank", checked);
    });

    auto *layout = new QVBoxLayout(tab);
    layout->addWidget(leadingBlank);
    layout->addWidget(hint);
    layout->addStretch(1);
    return tab;
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    m_tabs = new QTabWidget(this);
    m_biblePage = new BibleSettingsPage(m_tabs);
    m_tabs->addTab(m_biblePage, tr("Bibles"));
    m_tabs->addTab(createLibraryTab(m_tabs), tr("Media library"));
    m_tabs->addTab(createSlidesTab(m_tabs), tr("Slides"));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_tabs);
}

void SettingsPage::showTab(Tab tab)
{
    m_tabs->setCurrentIndex(tab);
}
