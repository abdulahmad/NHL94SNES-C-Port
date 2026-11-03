#!/usr/bin/env python3
"""The re-cut (one C file per asm module): fold whole C files into the file of the asm module that
holds their routines (tools/check_names.py --list other_module says which). Each source's code goes
after the target's, in the order given; its header's declarations after the target header's; its
#includes join the target's; game/game.h stops including the source header; the sources are
removed (git rm).

    python3 tools/merge_files.py game/play/logic game/play/assign game/play/checks game/play/input

(paths without .c / .h; the first is the target, which need not exist yet)."""
import os, re, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def split(text):
    """(first comment line, #include lines, the rest) of a C file or header"""
    lines = text.splitlines(keepends=True)
    head, incs, i = [], [], 0
    if lines and lines[0].startswith('/*'):
        while i < len(lines):
            head.append(lines[i]); i += 1
            if '*/' in head[-1]:
                break
    while i < len(lines) and (lines[i].startswith(('#include', '#pragma once')) or not lines[i].strip()):
        if lines[i].startswith('#include'):
            incs.append(lines[i])
        i += 1
    return ''.join(head), incs, ''.join(lines[i:])


def merge(target, sources, ext):
    tpath = os.path.join(ROOT, target + ext)
    nl = '\n'
    if os.path.exists(tpath):
        raw = open(tpath, newline='').read()
        nl = '\r\n' if '\r\n' in raw else '\n'
        head, incs, body = split(raw.replace('\r\n', '\n'))
    else:
        name = os.path.basename(target)
        head, incs, body = '/* %s (src/routines/%s). */\n' % (name, name), [], ''
    for s in sources:
        spath = os.path.join(ROOT, s + ext)
        if not os.path.exists(spath):
            continue
        _, sincs, sbody = split(open(spath, newline='').read().replace('\r\n', '\n'))
        for inc in sincs:
            own = '"%s%s"' % (os.path.basename(s), ext)
            if inc not in incs and own not in inc and '"%s.h"' % os.path.basename(target) not in inc:
                incs.append(inc)
        body = body.rstrip('\n') + '\n\n' + sbody.lstrip('\n') if body.strip() else sbody
    out = head + ('#pragma once\n' if ext == '.h' else '') + ''.join(incs) + '\n' + body.rstrip('\n') + '\n'
    open(tpath, 'w', newline='').write(out.replace('\n', nl))


def main():
    target, sources = sys.argv[1], sys.argv[2:]
    merge(target, sources, '.c')
    merge(target, sources, '.h')
    gh = os.path.join(ROOT, 'game', 'game.h')
    t = open(gh, newline='').read()
    tinc = '#include "%s.h"' % os.path.relpath(os.path.join(ROOT, target), os.path.join(ROOT, 'game')).replace(os.sep, '/')
    for s in sources:
        sinc = '#include "%s.h"' % os.path.relpath(os.path.join(ROOT, s), os.path.join(ROOT, 'game')).replace(os.sep, '/')
        if tinc in t:
            t = re.sub(r'^' + re.escape(sinc) + r'\r?\n', '', t, flags=re.M)
        else:
            t = t.replace(sinc, tinc)
    open(gh, 'w', newline='').write(t)
    for s in sources:
        for ext in ('.c', '.h'):
            if os.path.exists(os.path.join(ROOT, s + ext)):
                subprocess.run(['git', 'rm', '-q', s + ext], cwd=ROOT, check=True)
    subprocess.run(['git', 'add', target + '.c', target + '.h', 'game/game.h'], cwd=ROOT, check=True)
    print('merged', ', '.join(sources), 'into', target)


if __name__ == '__main__':
    main()
