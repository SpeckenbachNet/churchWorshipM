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

#include <QString>
#include <QStringList>

// Imports a SWORD bible module (the ZIP files from the CrossWire repository,
// e.g. https://crosswire.org/ftpmirror/pub/sword/packages/rawzip/GerNeUe.zip)
// into our own format (see BibleStore).
//
// Supported: ModDrv=zText with CompressType=ZIP, UTF-8 or Latin-1, OSIS/ThML/plain markup,
// versifications known to Versification::get().
// Not supported: encrypted modules (CipherKey), other compression types, rawText/zText4.
namespace SwordImporter {

// Names of all modules in a SWORD ZIP (one ZIP may contain several, e.g. the Offene Bibel)
QStringList modulesInZip(const QString &zipPath);

// Imports one module ('module' empty = the first one). Non-empty fields of 'hint'
// (id, name, abbreviation, license, source) override the values of the module.
// Returns the id of the imported bible, or empty with 'error' set.
QString importZip(const QString &zipPath, const BibleInfo &hint, const QString &module, QString *error);

// Removes OSIS/ThML markup: drops notes, returns section titles separately (public for testing)
QString cleanVerseMarkup(const QString &raw, QString *heading);

} // namespace SwordImporter
