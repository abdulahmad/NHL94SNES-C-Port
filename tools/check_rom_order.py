#!/usr/bin/env python3
"""ROM-order gate for one C module.

Compares original C defs in game/<module>.c with 94S globals under
src/routines/<module>/ that have a C function, in name_map address order.
Skips static C helpers, asm locals, and data labels (and unused asm with SYM_
but no C). C extras that are not asm globals in this module (Parent_x locals,
missing framehooks) hitchhike with the parent and do not fail. Does not fail
the whole tree: pass --module M.

Self-test: print 7/7 membership, not ROM order until S2.

  python3 tools/check_rom_order.py --module print
  python3 tools/check_rom_order.py --asm ../nhl94snes --module print
"""
import argparse
import csv
import os
import sys

import check_names as cn

ROOT = cn.ROOT


def find_asm(arg):
    name = 'NHL94SNES'
    cands = (
        arg,
        os.environ.get('NHL_ASM'),
        os.path.join(ROOT, '..', 'nhl94snes'),
        os.path.join(ROOT, '..', name),
        os.path.join(ROOT, 'ref', name),
    )
    for cand in cands:
        if cand and os.path.isdir(os.path.join(cand, 'src')):
            return os.path.normpath(cand)
    sys.exit('check_rom_order.py: no NHL94SNES checkout (give --asm, set NHL_ASM, '
             'or clone it beside this repository)')


def c_path(cdir, module):
    found = []
    for base, _, files in os.walk(cdir):
        if module + '.c' in files:
            found.append(os.path.join(base, module + '.c'))
    found.sort()
    if len(found) == 1:
        return found[0]
    if not found:
        sys.exit('check_rom_order.py: no %s.c under game/' % module)
    sys.exit('check_rom_order.py: multiple %s.c under game/' % module)


def routines_dir(asm, module):
    path = os.path.join(asm, 'src', 'routines', module)
    if os.path.isdir(path):
        return path
    sys.exit('check_rom_order.py: no src/routines/%s/ under %s' % (module, asm))


def nonstatic_originals(cdir, syms):
    """Non-static C defs that have SYM_, in source order per file."""
    by_file = {}
    all_names = set()
    for base, _, files in os.walk(cdir):
        for f in sorted(files):
            if not f.endswith('.c'):
                continue
            path = os.path.join(base, f)
            rel = os.path.relpath(path, ROOT).replace(os.sep, '/')
            names = []
            for line in open(path, encoding='utf-8', errors='replace'):
                if line.startswith((' ', '\t', '#', '/', '*', '}')):
                    continue
                if line.startswith(('static ', 'static\t')):
                    continue
                m = cn.DEF.match(line.rstrip('\r\n'))
                if m and m.group(1) not in cn.KEYWORDS and m.group(1) in syms:
                    names.append(m.group(1))
                    all_names.add(m.group(1))
            by_file[rel] = names
    return by_file, all_names


def asm_globals_with_c(asm, module, have_c):
    """94S globals in src/routines/<module>/, name_map address order, that have C."""
    prefix = 'src/routines/%s/' % module
    rows = []
    seen = set()
    path = os.path.join(asm, 'docs', 'name_map.csv')
    for row in csv.DictReader(open(path, encoding='utf-8')):
        name, f = row.get('name') or '', (row.get('file') or '').replace('\\', '/')
        if not name or not f.startswith(prefix):
            continue
        if name.startswith('.'):
            continue
        auto = row.get('auto_name') or ''
        if auto.startswith('DATA'):
            continue
        cname = cn.c_of(name)
        if cname not in have_c or cname in seen:
            continue
        try:
            addr = int((row.get('address') or '').lstrip('$'), 16)
        except ValueError:
            continue
        seen.add(cname)
        rows.append((addr, cname))
    rows.sort()
    return [c for _, c in rows]


def fmt(names):
    return ' '.join(names) if names else '(none)'


def check_module(module, cdir, asm, by_file, have_c):
    routines_dir(asm, module)
    c_file = c_path(cdir, module)
    rel = os.path.relpath(c_file, ROOT).replace(os.sep, '/')
    c_names = by_file.get(rel, [])
    a_names = asm_globals_with_c(asm, module, have_c)
    extra = [n for n in c_names if n not in set(a_names)]
    missing = [n for n in a_names if n not in set(c_names)]
    c_ord = [n for n in c_names if n in set(a_names)]
    if c_ord == a_names and not missing:
        msg = '%s: %d original routines, ROM order OK' % (module, len(a_names))
        if extra:
            msg += ' (%d hitchhikers)' % len(extra)
        print(msg)
        return 0
    print('%s: ROM order mismatch (%d C originals, %d asm with C)' % (
        module, len(c_ord), len(a_names)))
    if extra:
        print('  hitchhikers:  %s' % fmt(extra))
    if missing:
        print('  missing in C: %s' % fmt(missing))
    print('  asm: %s' % fmt(a_names))
    print('  c:   %s' % fmt(c_ord))
    return 1


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--asm')
    ap.add_argument('--module', action='append', metavar='M',
                    help='C / 94S routines folder (repeatable). Required.')
    a = ap.parse_args()
    if not a.module:
        ap.error('pass --module M (does not fail the whole tree; S2 checks one module)')

    cdir = cn.c_dir()
    syms = cn.symbols(cdir)
    asm = find_asm(a.asm)
    by_file, have_c = nonstatic_originals(cdir, syms)

    rc = 0
    for module in a.module:
        rc |= check_module(module, cdir, asm, by_file, have_c)
    return rc


if __name__ == '__main__':
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
