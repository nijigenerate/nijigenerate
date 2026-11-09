#!/usr/bin/env python3
"""Extract translation messages from the D and native C++ sources."""

import argparse
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
KEYWORDS = ("_", "__", "ngAutoRigMessage", "ngTranslate", "ngTranslateCString", "ngTranslateRetained", "translated")
STRING = r'(?:u8)?"(?:\\.|[^"\\])*"'
WRAPPER = re.compile(
    r'(?<![\w:])(?:[\w]+::)*(?:fromStaticLiteral|fromStaticView|RetainedStringSlice|text)'
    r'\s*\(\s*((?:' + STRING + r'\s*)+)\)', re.DOTALL)
IGNORED = re.compile(r'//[^\n]*|/\*.*?\*/|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)


def unwrap_literals(source):
    # Keep original line numbers, and never rewrite text inside comments or strings.
    while True:
        masked = bytearray(len(source))
        for match in IGNORED.finditer(source):
            masked[match.start():match.end()] = b'\1' * (match.end() - match.start())
        matches = [m for m in WRAPPER.finditer(source) if not masked[m.start()]]
        if not matches:
            return source
        for match in reversed(matches):
            replacement = match.group(1)
            replacement += '\n' * (match.group(0).count('\n') - replacement.count('\n'))
            source = source[:match.start()] + replacement + source[match.end():]


def run_xgettext(paths, language, output, directory, join=False):
    if not paths:
        return
    args = ['xgettext', '--language=' + language, '--from-code=UTF-8', '--add-comments',
            '--keyword', '--sort-output', '--output=' + str(output)]
    args.extend('--keyword=' + name for name in KEYWORDS)
    if join:
        args.append('--join-existing')
    listing = directory / (language + '-sources.txt')
    listing.write_text(''.join(str(p) + '\n' for p in paths), encoding='utf-8')
    subprocess.run(args + ['--files-from=' + str(listing)], cwd=directory, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, default=ROOT / 'tl/template.pot')
    args = parser.parse_args()
    output = args.output.resolve()
    with tempfile.TemporaryDirectory(prefix='nijigenerate-pot-') as temp:
        scratch = pathlib.Path(temp)
        d_paths = sorted(p.relative_to(ROOT) for root in ('source', 'tests', 'vendor')
                         for p in (ROOT / root).rglob('*.d'))
        for path in d_paths:
            target = scratch / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.symlink_to(ROOT / path)
        cpp_paths = sorted(p.relative_to(ROOT) for p in (ROOT / 'native/src').rglob('*')
                           if p.suffix in ('.cpp', '.hpp', '.h', '.mm'))
        for path in cpp_paths:
            target = scratch / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(unwrap_literals((ROOT / path).read_text(encoding='utf-8')), encoding='utf-8')
        pot = scratch / 'template.pot'
        run_xgettext(d_paths, 'D', pot, scratch)
        run_xgettext(cpp_paths, 'C++', pot, scratch, join=True)
        output.write_bytes(pot.read_bytes())


if __name__ == '__main__':
    main()
