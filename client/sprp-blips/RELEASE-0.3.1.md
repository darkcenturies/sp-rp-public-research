# Doctor Valkyrie 0.3.1

Copy `doctor-valkyrie.asi` into the Project Eagle game folder beside
`gta_sa.exe`. Remove older copies of Doctor Valkyrie first; do not keep both
0.3.0 and 0.3.1 under different filenames.

The ASI owns its diagnosis log and window while preserving original Project
Eagle and CLEO logs. It works offline and embeds all 35 Project Eagle rules,
all 331 supplied CrashList rules, and aligned Spanish text for all 366.

This package includes:

- `doctor-valkyrie.asi` — version 0.3.1, hardened x86 release build
- `README.md` and `DOCTOR-VALKYRIE.md` — installation and behavior
- `CHANGELOG-0.3.0.md` — the previous full release change list
- `CHANGELOG-0.3.1.md` — changes since 0.3.0
- `CRASHDETECT-COVERAGE.md` — audited CrashList/CrashDetect coverage boundary

CrashDetect remains a separate SA-MP server development plugin. Its Pawn VM
features cannot operate inside this client ASI; see the coverage document for
the exact comparison.
