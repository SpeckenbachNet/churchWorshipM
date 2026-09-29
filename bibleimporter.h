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

// Converts bible files of different formats into our own format (see BibleStore).
//
// Detected automatically:
//   *.zip with mods.d/     SWORD module (see SwordImporter)
//   *.zip otherwise        first *.xml / *.json inside is imported
//   *.json                 getBible.net v2
//   *.xml  <osis>          OSIS
//   *.xml  <XMLBIBLE>      Zefania XML
//
// Non-empty fields of 'hint' (id, name, abbreviation, license, ...) override the file's own values.
// Returns the id of the imported bible, or empty with 'error' set.
// Runs synchronously; call it from a worker thread for big files.
namespace BibleImporter {

QString importFile(const QString &path, const BibleInfo &hint, const QString &swordModule, QString *error);

QString importGetBible(const QString &path, const BibleInfo &hint, QString *error);
QString importOsis(const QString &path, const BibleInfo &hint, QString *error);
QString importZefania(const QString &path, const BibleInfo &hint, QString *error);

// File name based id for bibles without one ("Luther 1912.xml" -> "luther_1912")
QString idFromFileName(const QString &path);

} // namespace BibleImporter
