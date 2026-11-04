#!/usr/bin/env python3
"""Rename original routines to the asm's current labels (the names gate's "renamed" rows): the C
function and its SYM_ address names (SYM_<name> and its locals SYM_<name>_x, and the C functions <name>_x
for those locals), everywhere in game/,
platform/ and test/. A chain or swap of names (a renamed to b, c renamed to a) is safe: every name
goes through a placeholder first.

    python3 tools/check_names.py                       # writes out/names_status.csv
    python3 tools/rename_routines.py --module hockey   # the rows of one asm module
    python3 tools/rename_routines.py --module hockey --dry-run

--skip NAME leaves a row alone (a new name that is already another C identifier)."""
import argparse, csv, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--module', action='append', required=True)
    ap.add_argument('--skip', action='append', default=[])
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    rows = [r for r in csv.DictReader(open(os.path.join(ROOT, 'out', 'names_status.csv')))
            if r['status'] == 'renamed' and r['asm_module'] in a.module and r['function'] not in a.skip]
    pairs = [(r['function'], r['c_name']) for r in rows if r['function'] != r['c_name']]
    if not pairs:
        print('nothing to rename')
        return 0
    files = [os.path.join(b, f) for d in ('game', 'platform', 'test') for b, _, fs in os.walk(os.path.join(ROOT, d))
             for f in fs if f.endswith(('.c', '.h'))]
    subs = []
    for i, (old, new) in enumerate(pairs):
        tmp = '__RENAME_%d__' % i
        # the function, its SYM_ address and the SYM_ addresses of its locals (SYM_old_x)
        subs.append((re.compile(r'\b(SYM_)?%s\b' % re.escape(old)), r'\1' + tmp, re.compile(r'\b%s\b' % tmp), new))
        # its locals: SYM_old_x and the C functions old_x that stand for them (a routine's locals keep
        # their place under it: the clock's table pairs each C function with its SYM_)
        subs.append((re.compile(r'\b(SYM_)?%s_(\w+)' % re.escape(old)), r'\1' + tmp + r'_\2', None, None))
    changed = 0
    for f in files:
        t = open(f, encoding='utf-8', newline='').read()
        u = t
        for pat, rep, _, _ in subs:
            u = pat.sub(rep, u)
        for i, (old, new) in enumerate(pairs):
            u = u.replace('__RENAME_%d__' % i, new)
        if u != t:
            changed += 1
            if not a.dry_run:
                open(f, 'w', encoding='utf-8', newline='').write(u)
    for old, new in pairs:
        print('%-30s -> %s' % (old, new))
    print('%d names, %d files%s' % (len(pairs), changed, ' (dry run)' if a.dry_run else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
