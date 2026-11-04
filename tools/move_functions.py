#!/usr/bin/env python3
"""The re-cut, a function at a time: move routines (each with the comment above it) from one C
file to the end of another, and their prototypes from one header to the other.

    python3 tools/move_functions.py game/screen/replay game/play/logic freezewindow checkwindow
    python3 tools/move_functions.py SRC DST --from-names out/names_status.csv --module logic

(paths without .c / .h). A static helper or macro the moved code needs stays where it was: the
build says which; move it too (by name) or give it to both."""
import argparse, csv, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def load(path):
    raw = open(path, newline='').read()
    return raw.replace('\r\n', '\n'), '\r\n' if '\r\n' in raw else '\n'


def save(path, text, nl):
    open(path, 'w', newline='').write(text.replace('\n', nl))


def take(text, name):
    """(text without the definition of name and its comment, the definition) or None"""
    lines = text.split('\n')
    start = None
    for i, l in enumerate(lines):
        if re.match(r'^(static\s+)?[A-Za-z_][\w \*]*?\b%s\s*\(' % re.escape(name), l) and not l.rstrip().endswith(';'):
            start = i
            break
    if start is None:
        return None
    end = start
    head = lines[start].split('/*')[0]                   # a one-line body: braces balanced on the head's line
    if not ('{' in head and head.count('{') == head.count('}')):
        while end < len(lines) and lines[end] != '}':
            end += 1
    first = start                                         # the comment block right above
    while first > 0 and (lines[first - 1].startswith(('/*', ' *', '*/'))):
        first -= 1
        if lines[first].startswith('/*'):
            break
    body = '\n'.join(lines[first:end + 1])
    rest = lines[:first] + lines[end + 1:]
    while first < len(rest) and first > 0 and rest[first - 1] == '' and rest[first] == '':
        del rest[first]
    return '\n'.join(rest), body


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('src')
    ap.add_argument('dst')
    ap.add_argument('names', nargs='*')
    ap.add_argument('--module')
    a = ap.parse_args()
    names = list(a.names)
    if a.module:
        for r in csv.DictReader(open(os.path.join(ROOT, 'out', 'names_status.csv'))):
            if r['c_file'] == a.src + '.c' and r['status'] == 'other_module' and r['asm_module'] == a.module:
                names.append(r['function'])
    sc, snl = load(os.path.join(ROOT, a.src + '.c'))
    dpath = os.path.join(ROOT, a.dst + '.c')
    if os.path.exists(dpath):
        dc, dnl = load(dpath)
    else:
        m = os.path.basename(a.dst)
        dc, dnl = '/* %s (src/routines/%s). */\n#include "game.h"\n' % (m, m), snl
    sh, shnl = load(os.path.join(ROOT, a.src + '.h'))
    hpath = os.path.join(ROOT, a.dst + '.h')
    if os.path.exists(hpath):
        dh, dhnl = load(hpath)
    else:
        m = os.path.basename(a.dst)
        dh, dhnl = '/* %s.c (src/routines/%s). */\n#pragma once\n#include <stdint.h>\n' % (m, m), shnl
    moved = []
    for n in names:
        r = take(sc, n)
        if r is None:
            print('not found:', n)
            continue
        sc, body = r
        dc = dc.rstrip('\n') + '\n\n' + body + '\n'
        protos = [l for l in sh.split('\n') if re.search(r'\b%s\s*\(' % re.escape(n), l) and l.rstrip().endswith(';')]
        for p in protos:
            sh = sh.replace(p + '\n', '', 1)
            dh = dh.rstrip('\n') + '\n' + p + '\n'
        moved.append(n)
    save(os.path.join(ROOT, a.src + '.c'), sc, snl)
    save(dpath, dc, dnl)
    save(os.path.join(ROOT, a.src + '.h'), sh, shnl)
    save(hpath, dh, dhnl)
    print('moved %d to %s: %s' % (len(moved), a.dst, ' '.join(moved)))


if __name__ == '__main__':
    sys.exit(main())
