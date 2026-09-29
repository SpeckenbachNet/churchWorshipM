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
#include "biblecatalog.h"

#include <QCoreApplication>

BibleInfo BibleCatalogEntry::hint() const
{
    BibleInfo info;
    info.id = id;
    info.name = name;
    info.abbreviation = abbreviation;
    info.language = QStringLiteral("de");
    info.license = license;
    info.source = url;
    return info;
}

namespace BibleCatalog {

QList<BibleCatalogEntry> entries()
{
    const QString publicDomain = QCoreApplication::translate("BibleCatalog", "Public domain");
    const QString crossWire = QStringLiteral("https://crosswire.org/ftpmirror/pub/sword/packages/rawzip/");
    const QString getBible  = QStringLiteral("https://api.getbible.net/v2/");

    // Sources verified 2026-09: CrossWire (SWORD), open-bibles (OSIS), getBible (JSON), offene-bibel.de (SWORD)
    return {
        {"gerneue", "Neue evangelistische Übersetzung", "NeÜ",
         QCoreApplication::translate("BibleCatalog",
             "© Karl-Heinz Vanheiden – free use in bible software, distributed by CrossWire"),
         crossWire + "GerNeUe.zip", {}, {}, true},
        {"germenge", "Menge-Bibel (1939)", "Menge",
         publicDomain, crossWire + "GerMenge.zip", {}, {}, false},
        {"luther1912", "Luther 1912", "LUT1912",
         publicDomain,
         "https://raw.githubusercontent.com/seven1m/open-bibles/master/deu-luther1912.osis.xml", {}, {}, false},
        {"elberfelder1905", "Elberfelder (1905)", "ELB1905",
         publicDomain, getBible + "elberfelder1905.json", {}, {}, false},
        {"offbile", "Offene Bibel – Lesefassung", "OfBi",
         QCoreApplication::translate("BibleCatalog", "CC BY-SA 3.0 – name “Offene Bibel” as source"),
         "https://offene-bibel.de/mediawiki/images/0/0f/OffBi_SWORD_2018-12-15.zip", "OffBiLe",
         QCoreApplication::translate("BibleCatalog", "Translation in progress, not all books are available yet."),
         false},
        {"elberfelder1871", "Elberfelder (1871)", "ELB1871",
         publicDomain, getBible + "elberfelder.json", {}, {}, false},
        {"luther1545", "Luther (1545)", "LUT1545",
         publicDomain, getBible + "luther1545.json", {}, {}, false},
        {"schlachter1951", "Schlachter (1951)", "SCH1951",
         QCoreApplication::translate("BibleCatalog", "© Genfer Bibelgesellschaft – non-commercial use only"),
         getBible + "schlachter.json", {}, {}, true},
    };
}

} // namespace BibleCatalog
