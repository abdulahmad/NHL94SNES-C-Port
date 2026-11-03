#!/usr/bin/env python3
"""Move one asm module's routines out of a C file (tools/move_functions.py --module), then settle
the static helpers the build reports: a helper only the moved code calls goes with it (ahead of the
first moved routine); one both files call stops being static and gets a prototype in the source's
header. Anything else (a static variable, a macro) is printed for a hand to do.

    python3 tools/recut_module.py game/system/snes game/system/middle middle"""
import os, re, subprocess, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(ROOT, 'tools'))
import move_functions as mf  # noqa: E402


def build():
    r = subprocess.run(['make', 'ROM=' + os.environ.get('ROM', '/home/claude/roms/nhl94.sfc'), '-j8'], cwd=ROOT,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    return r.returncode, r.stdout


def main():
    src, dst, module = sys.argv[1:4]
    subprocess.run(['python3', 'tools/check_names.py'], cwd=ROOT, stdout=subprocess.DEVNULL)
    subprocess.run(['python3', 'tools/move_functions.py', src, dst, '--module', module], cwd=ROOT, check=True)
    for _ in range(30):
        rc, out = build()
        implicit = sorted(set(re.findall(r"implicit declaration of function ['\u2018](\w+)['\u2019]", out)))
        if rc == 0 and not implicit:
            print('builds, no warnings' if 'warning:' not in out else 'builds, with warnings:\n' + '\n'.join(l for l in out.splitlines() if 'warning:' in l))
            return 0
        if not implicit:
            print('\n'.join(l for l in out.splitlines() if 'error' in l or 'warning:' in l)[:3000])
            return 1
        sc, snl = mf.load(os.path.join(ROOT, src + '.c'))
        dc, dnl = mf.load(os.path.join(ROOT, dst + '.c'))
        sh, shnl = mf.load(os.path.join(ROOT, src + '.h'))
        for name in implicit:
            r = mf.take(sc, name)
            if r is None:
                print('not in %s.c: %s' % (src, name))
                return 1
            rest, body = r
            if re.search(r'\b%s\s*\(' % re.escape(name), rest):  # still called in the source: share it
                defline = next(l for l in body.split('\n') if re.match(r'^(static\s+)?[A-Za-z_][\w \*]*?\b%s\s*\(' % re.escape(name), l))
                newdef = re.sub(r'^static\s+(inline\s+)?', '', defline)
                sc = sc.replace(defline, newdef, 1)
                proto = re.sub(r'\s*\{.*$', '', newdef).rstrip() + ';'
                sh = sh.rstrip('\n') + '\n' + proto + '\n'
                print('shared:', name)
            else:                                         # only the moved code: it goes along
                sc = rest
                lines = dc.split('\n')
                k = next(i for i, l in enumerate(lines) if re.search(r'\b%s\s*\(' % re.escape(name), l))
                while k > 0 and not re.match(r'^[A-Za-z_][\w \*]*\(', lines[k]):
                    k -= 1                                # up to the definition holding the first call
                while k > 0 and (lines[k - 1].startswith(('/*', ' *')) or lines[k - 1].rstrip().endswith('*/')):
                    k -= 1                                # and over its comment
                    if lines[k].startswith('/*'):
                        break
                lines[k:k] = body.split('\n') + ['']
                dc = '\n'.join(lines)
                print('moved along:', name)
        mf.save(os.path.join(ROOT, src + '.c'), sc, snl)
        mf.save(os.path.join(ROOT, dst + '.c'), dc, dnl)
        mf.save(os.path.join(ROOT, src + '.h'), sh, shnl)
    return 1


if __name__ == '__main__':
    sys.exit(main())
