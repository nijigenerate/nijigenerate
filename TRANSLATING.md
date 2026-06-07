# Translating nijigenerate
nijigenerate uses gettext to handle translation, currently pluralization is not support but will be added soon(TM).  
You'll need a distribution of gettext to work on translation files.  
Currently the language support within nijigenerate is limited, as such there may be rendering errors for some languages.  
We do not support languages that are right-to-left due to limitations within our UI library.  

&nbsp;
&nbsp;

## Creating a translation file for a new language
To create a new translation file, run
```sh
msginit --locale=<langcode> --input=tl/template.pot -o tl/<langcode>.po
```
replace `<langcode>` with your language's language code.

#### NOTE
 * Make sure to update the charset variable in your .po file to UTF-8. nijigenerate only supports UTF-8.

&nbsp;
&nbsp;

## Merging information from latest template
```sh
msgmerge -o tl/<langcode>_merged.po tl/<langcode>.po tl/template.pot
```

Check if the merges make sense, if so replace `<langcode>.po` with `<langcode>_merged.po`.

&nbsp;
&nbsp;

## Validate Your Translation
Our project uses material icons and includes fmtstr to avoid crashes when loading po or missing icon symbols. Please use the following tools to check
```sh
# install dependencies
pip install babel
# validate
python translation-validator.py -f tl/<langcode>.po
# or validate all
python translation-validator.py -a
```

&nbsp;
&nbsp;

## Importing Translations from Another .po File
`po_import.py` merges translations from a source `.po` into a target `.po` **without touching the target's formatting, comments, or whitespace**.

```sh
# Fill in only empty msgstr entries (safe, non-destructive)
python3 po_import.py SOURCE.po tl/<langcode>.po

# Write result to a new file instead of modifying the target in-place
python3 po_import.py SOURCE.po tl/<langcode>.po --out output.po

# Also overwrite non-empty msgstr entries that differ from the source
python3 po_import.py SOURCE.po tl/<langcode>.po --overwrite

# Include fuzzy entries from the source
python3 po_import.py SOURCE.po tl/<langcode>.po --use-fuzzy
```

> **Note:** Fuzzy source entries are skipped by default. The script never modifies comments, `#:` references, flags, or blank lines in the target file.

&nbsp;
&nbsp;

## I need to reorder format parameters
To specify which format parameter you're indexing, use the `<index>$` operator.  
Eg. `%2$s` will index the second entry of the format string.

&nbsp;
&nbsp;

## Generating output file
```
msgfmt tl/<langcode>.po -o <langcode>.mo
```
The `<langcode>.mo` file can be put in the configuration directory, or next to the executable for testing.

&nbsp;
&nbsp;

# Translation Storage
Final translations will be stored in the .nijigenerate folder, which resides in
 * `~/.config/.nijigenerate` on Linux
 * `~/Library/Application Support/.nijigenerate` on macOS
 * `%AppData%/.nijigenerate` on Windows

But nijigenerate will also try to locate translation files in the current executable directory, as well as the current working directory.