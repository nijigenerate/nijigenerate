#!/usr/bin/env python3
import functools
import sys
import glob
import argparse

from babel.messages.pofile import read_po

"""
# polib is broken, use babel instead
# gettext not supported .po files

pip install babel opencc-python-reimplemented
"""

tl_files = glob.glob("tl/*.po")

ignore = [
    "tl/template.pot",
]

fmt_str_keywords = ['%s', '%d', '%f', '%u', '%lu']
check_fuzzy = False
ignore_empty = False

def validate_string_formatting(msgid, msgstr) -> bool:
    """
    Check if the string formatting is correct
    Note: currently not checking the order of the formatting, and not check %2$s etc
    """
    fmt_str_count = {}
    fmt_target_count = {}
    for fmt_str in fmt_str_keywords:
        fmt_str_count[fmt_str] = msgid.count(fmt_str)
        fmt_target_count[fmt_str] = msgstr.count(fmt_str)

    for fmt_str in fmt_str_keywords:
        if fmt_str_count[fmt_str] == 0:
            continue
        if fmt_str_count[fmt_str] != fmt_target_count[fmt_str]:
            return False
        
    return True

def validate_non_ascii(msgstr, msgid) -> bool:
    """
    For example, English using non-ascii as icon (Google Material Icons)
    """
    non_ascii = [c for c in msgid if ord(c) > 127]
    non_ascii_target = [c for c in msgstr  if ord(c) > 127]
    
    for c in non_ascii:
        if c not in non_ascii_target:
            return False
    
    return True

class ValidationError(Exception):
    def __init__(self, message, entry):
        self.message = message
        self.entry = entry
        
    def __str__(self):
        return self.message

class Summary:
    def __init__(self):
        self.total = 0
        self.success = 0

    def print_summary(self):
        print(f"Total: {self.total}, translated rate: {self.success}/{self.total} ({self.success/self.total*100:.2f}%)")

def validate_string(entry, summary : Summary) -> bool:
    summary.total += 1
    if entry.string == "" and not ignore_empty:
        raise ValidationError("msgstr is empty", entry)
    
    if entry.string.startswith('\ue894'):
        raise ValidationError("msgstr contains untranslated marker (U+E894)", entry)

    if not validate_string_formatting(entry.id, entry.string):
        raise ValidationError("msgstr fmtstr is incorrect", entry)
    
    if not validate_non_ascii(entry.string, entry.id):
        raise ValidationError("msgstr may lost icon", entry)

    if entry.fuzzy and check_fuzzy:
        raise ValidationError("msgstr is fuzzy", entry)

    if not entry.fuzzy and entry.string != "":
        summary.success += 1

    return True

escape_chars = {
    "\n": "\\n", "\r": "\\r", "\t": "\\t",
    "\"": "\\\""
}
escape_table = str.maketrans(escape_chars)

def escape_string(s) -> str:
    return s.translate(escape_table)

# ── Traditional Chinese checker ──────────────────────────────────────────────

TRAD_CHECK_THRESHOLD = 0.15   # flag if fewer than 15% of CJK chars changed

def _cjk_chars(s: str) -> list[str]:
    """Return only the CJK unified ideograph characters in s."""
    return [c for c in s if '\u4e00' <= c <= '\u9fff']

def check_trad_file(file_path: str, threshold: float = TRAD_CHECK_THRESHOLD) -> None:
    try:
        import opencc
    except ImportError:
        print("opencc-python-reimplemented is required: pip install opencc-python-reimplemented")
        sys.exit(1)

    cc = opencc.OpenCC('tw2sp')
    suspicious = []

    with open(file_path, 'r', encoding='utf-8') as f:
        catalog = read_po(f)

    for entry in catalog:
        msgstr = entry.string
        if not msgstr:
            continue

        cjk = _cjk_chars(msgstr)
        if len(cjk) < 3:       # too short to be meaningful
            continue

        converted = cc.convert(msgstr)
        changed = sum(a != b for a, b in zip(msgstr, converted) if '\u4e00' <= a <= '\u9fff')
        ratio = changed / len(cjk)

        if ratio < threshold:
            suspicious.append((entry, ratio, converted))

    if not suspicious:
        print(f"{file_path}: all entries pass zh-TW locale consistency check.")
        return

    print(f"\n{'='*60}")
    print(f"Entries that may not follow zh-TW locale and phrasing conventions: {file_path}")
    print(f"(detected low rate of expected zh-TW orthographic variation: < {threshold*100:.0f}%)")
    print(f"{'='*60}")
    for entry, ratio, converted in suspicious:
        print(f"  Line {entry.lineno}  [Variation rate: {ratio*100:.0f}%]")
        print(f"    msgid:     {escape_string(entry.id)}")
        print(f"    msgstr:    {escape_string(entry.string)}")
        print(f"    reference: {escape_string(converted)}")
        print()

@functools.lru_cache
def load_template_msgid() -> set:
    template_catalog = read_po(open("tl/template.pot", 'r', encoding='utf-8'))
    return set([entry.id for entry in template_catalog])

def check_is_latest_template(tl_catalog) -> bool:
    """
    Check if the translation file is the latest template
    """
    template_msgid = load_template_msgid()
    success = True
    
    for msgid in template_msgid:
        if msgid in tl_catalog:
            continue
            
        if msgid == "":
            continue

        print(f"\tMissing msgid: '{escape_string(msgid)}'")
        success = False
        
    return success

def validate_file(file_path) -> int:
    ret_code = 0
    summary = Summary()
    print("Validating file: " + file_path)

    with open(file_path, 'r', encoding='utf-8') as file:
        catalog = read_po(file)

    for entry in catalog:
        try:
            validate_string(entry, summary)
        except ValidationError as e:
            msgid = escape_string(e.entry.id)
            msgstr = escape_string(e.entry.string)
            print("Validation Error: " + str(e) + f" ({file_path}:{e.entry.lineno})")
            print(f"\tmsgid: \"{msgid}\"")
            print(f"\tmsgstr: \"{msgstr}\"")
            ret_code = 1

    summary.print_summary()
    
    if not check_is_latest_template(catalog):
        print(f"Warning: {file_path} may not be the latest template")
        print("Please update the translation file with the latest template")
        print(f"\tmsgmerge -o {file_path}_merged.po {file_path} tl/template.pot")
        ret_code = 1

    return ret_code

def validate_all() -> int:
    ret_code = 0
    for tl_file in tl_files:
        ret_code += validate_file(tl_file)
        
    return ret_code

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Validate translation files')
    parser.add_argument('-a', '--all', action='store_true', help='Validate all files')
    parser.add_argument('-f', '--file', type=str, help='Validate specific file')
    parser.add_argument('--fuzzy', action='store_true', help='Check fuzzy entries')
    parser.add_argument('--ignore-empty', action='store_true', help='Ignore empty msgstr')
    parser.add_argument('-t', '--check-trad', action='store_true',
                        help='Check zh-TW locale consistency via orthographic heuristic (OpenCC)')
    parser.add_argument('--trad-threshold', type=float, default=TRAD_CHECK_THRESHOLD,
                        help=f'Orthographic variation threshold for locale consistency check (default: {TRAD_CHECK_THRESHOLD})')
    args = parser.parse_args()
    
    if args.fuzzy:
        check_fuzzy = True
        
    if args.ignore_empty:
        ignore_empty = True

    if args.check_trad:
        if not args.file:
            print("--check-trad requires -f <file>")
            sys.exit(1)
        check_trad_file(args.file, threshold=args.trad_threshold)
        sys.exit(0)

    if args.all:
        sys.exit(validate_all())
    elif args.file:
        sys.exit(validate_file(args.file))
    else:
        print("No arguments given")
        parser.print_help()
        sys.exit(1)

    
