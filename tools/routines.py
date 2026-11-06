"""docs/ROUTINES.md from the game's own comments: every function in game/, the original routine it
is (a function named after one of the disassembly's labels, with that label's cartridge address
from game/symbols_gen.h), and what it does (the first sentence of the comment before it).
Run from the repository's root after changing the game: python tools/routines.py"""
import os
import re

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
GAME = os.path.join(ROOT, 'game')
DEF = re.compile(r'^(?:static\s+)?(?:inline\s+)?[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;]*\)\s*\{?\s*$')
SYM = re.compile(r'^#define SYM_(\w+) 0x([0-9A-F]+)u')


def first_sentence(text):
    text = ' '.join(text.split())
    m = re.match(r'(.+?[.!?])(\s|$)', text)
    return (m.group(1) if m else text).replace('|', '\\|')


def routines(path):
    lines = open(path, encoding='utf-8').read().splitlines()
    out = []
    for i, line in enumerate(lines):
        m = DEF.match(line)
        if not m or line.startswith((' ', '\t', '#', '/', '*', '}')) or m.group(1) in ('if', 'while', 'for', 'switch', 'return'):
            continue
        if not (line.rstrip().endswith('{') or (i + 1 < len(lines) and lines[i + 1].startswith('{'))):
            continue
        j, block = i - 1, []
        while j >= 0 and (lines[j].lstrip().startswith(('*', '/*')) or lines[j].rstrip().endswith('*/')):
            block.insert(0, lines[j].strip())
            if lines[j].lstrip().startswith('/*'): break
            j -= 1
        comment = ' '.join(b.lstrip('/*').rstrip('*/').strip() for b in block)
        out.append((m.group(1), first_sentence(comment) if comment else '', line.startswith('static')))
    return out


syms = {}
for line in open(os.path.join(GAME, 'symbols_gen.h'), encoding='utf-8'):
    s = SYM.match(line)
    if s: syms[s.group(1)] = s.group(2)
total = labelled = 0
md = ['# The routines', '',
      'Every function in `game/`, by file: the C name, the original routine it is (the',
      'disassembly\'s label of the same name and its cartridge address), and what it does. Generated from',
      'the code\'s own comments by `tools/routines.py`. A helper is a piece split out of an original',
      'routine, a part a routine shares with others, or a small function of the C\'s own.', '']
body = []
for f in sorted(os.listdir(GAME)):
    if not f.endswith('.c'): continue
    rs = routines(os.path.join(GAME, f))
    if not rs: continue
    body += ['### `game/%s` (%d)' % (f, len(rs)), '', '| Function | Original | What it does |', '|---|---|---|']
    for name, what, static in rs:
        orig = name in syms and not static
        body.append('| `%s` | %s | %s |' % (name, '`$%s`' % syms[name] if orig else '(a helper)', what))
        total += 1
        labelled += orig
    body.append('')
md += ['%d functions: %d original routines, %d helpers.' % (total, labelled, total - labelled), ''] + body
open(os.path.join(ROOT, 'docs', 'ROUTINES.md'), 'w', encoding='utf-8', newline='\n').write('\n'.join(md))
print('docs/ROUTINES.md: %d functions, %d original routines, %d helpers' % (total, labelled, total - labelled))
