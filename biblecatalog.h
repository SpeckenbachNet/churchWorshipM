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

#include "biblestore.h"

#include <QList>
#include <QString>

// Bibles offered for download in the settings.
// Only translations that may be used freely, or whose publisher allows the use in bible
// software, are listed. Everything is downloaded from the original source, we host nothing.
struct BibleCatalogEntry {
    QString id;             // id of the installed bible (= file name in BibleStore)
    QString name;
    QString abbreviation;
    QString license;        // shown in the list
    QString url;
    QString swordModule;    // module name if the ZIP contains several SWORD modules
    QString note;           // additional information (e.g. "incomplete")
    bool    needsConsent = false;   // not public domain: ask before downloading

    // Values handed to the importer (override the file's metadata)
    BibleInfo hint() const;
};

namespace BibleCatalog {

QList<BibleCatalogEntry> entries();

} // namespace BibleCatalog
