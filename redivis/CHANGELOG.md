# Changelog

Releases of Redivis's fork of ReadStat. Each version's section here becomes its release notes; see
[README.md](README.md) for how releases are made.

## 0.6.0

- Merge upstream `dev` (v1.1.9 and 148 later commits), including memory safety fixes found by fuzzing. The fork's
  fixes to `extract_metadata` that upstream hasn't made are kept: it ignores an unparseable numeric format, a missing
  value label set, and missing value ranges, rather than failing.
- `readstat --input-format sas7bdat - <output>` reads a SAS7BDAT file from standard input in a single forward pass, and
  `--metadata <file.json>` writes each variable's name, type, format and label as it goes. CSV output of SAS files
  writes dates as `yyyy-MM-dd` (with `HH:mm:ss` unless midnight) and numbers as integers or their shortest exact
  decimal, as Redivis's previous SAS converter did.
- Fix stateful encodings (e.g. Windows-1258) carrying each string's last character into the next one.
- Read SAS7BDAT files in MacRoman, MacUkrainian, MacIcelandic, CP942 and CP1381, through glibc's equivalents.
- Releases include `readstat` and `extract_metadata` binaries for linux/amd64 and linux/arm64.
