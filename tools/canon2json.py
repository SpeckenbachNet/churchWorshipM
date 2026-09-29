#!/usr/bin/env python3
"""
Generates ressources/bible/versifications.json from the SWORD versification headers.

Source files (CrossWire SWORD project, GPL-2):
  https://crosswire.org/svn/sword/trunk/include/canon.h          (KJV)
  https://crosswire.org/svn/sword/trunk/include/canon_german.h   (German, NT = KJV)
  https://crosswire.org/svn/sword/trunk/include/canon_luther.h   (Luther, incl. apocrypha)

Only the chapter/verse counts are taken over.

Usage: python3 tools/canon2json.py <dir with canon*.h> ressources/bible/versifications.json
"""
import json
import re
import sys


def parse(path):
    src = open(path, encoding='utf-8').read()
    books = {}
    for m in re.finditer(r'struct sbook (\w+)\[\]\s*=\s*\{(.*?)\n\};', src, re.S):
        entries = re.findall(r'\{"([^"]*)",\s*"([^"]*)",\s*"([^"]*)",\s*(\d+)\}', m.group(2))
        books[m.group(1)] = [(osis, int(chapters)) for _, osis, _, chapters in entries if osis]
    verses = {}
    for m in re.finditer(r'int (vm\w*)\[\]\s*=\s*\{(.*?)\n\};', src, re.S):
        body = re.sub(r'//[^\n]*', '', m.group(2))
        verses[m.group(1)] = [int(x) for x in re.findall(r'\d+', body)]
    return books, verses


def build(ot, nt, vm):
    out = {'ot': [], 'nt': []}
    i = 0
    for key, books in (('ot', ot), ('nt', nt)):
        for osis, chapters in books:
            out[key].append([osis, vm[i:i + chapters]])
            i += chapters
    assert i == len(vm), 'verse table does not match the book list'
    return out


def main():
    src, target = sys.argv[1], sys.argv[2]
    kjv_b, kjv_v = parse(f'{src}/canon.h')
    ger_b, ger_v = parse(f'{src}/canon_german.h')
    lut_b, lut_v = parse(f'{src}/canon_luther.h')
    data = {
        'KJV':    build(kjv_b['otbooks'], kjv_b['ntbooks'], kjv_v['vm']),
        'German': build(ger_b['otbooks_german'], kjv_b['ntbooks'], ger_v['vm_german']),
        'Luther': build(lut_b['otbooks_luther'], lut_b['ntbooks_luther'], lut_v['vm_luther']),
    }
    with open(target, 'w') as f:
        json.dump(data, f, separators=(',', ':'))


if __name__ == '__main__':
    main()
