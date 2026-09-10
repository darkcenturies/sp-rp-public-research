# Doctor Valkyrie 0.3.0

Doctor Valkyrie is an offline crash and CLEO-error reporter for Project Eagle.
Install `doctor-valkyrie.asi` in the main Project Eagle game folder. It creates
complete reports under `ProjectEagle_crashes` and shows the report immediately;
it does not replace, shorten, upload, or delete the game's existing logs.

## What is included

- `doctor-valkyrie.asi` — the complete 32-bit plugin; all rules, translations,
  artwork and footer marks are embedded. No loose database is required.
- `README.md` — this installation and behavior guide.
- `CHANGELOG.md` — the complete change list from 0.2.9 to 0.3.0.

## Install and test

1. Close GTA, Project Eagle and the S&SMP launcher completely.
2. Back up or remove an older `doctor-valkyrie.asi` so only one copy can load.
3. Copy the new `doctor-valkyrie.asi` into the main Project Eagle folder, beside
   `gta_pe.exe` and `PECore.asi`.
4. Start the game normally. Do not run duplicate copies from Mod Loader.

The report title and footer must show `0.3.0`. The Español button translates the
entire diagnosis, other findings, controls and readable report labels offline.
Addresses, paths, module names, filenames and registers remain unchanged so a
shared report is still technically useful. Supplied web addresses are clickable.

The rule database contains 35 Project Eagle diagnoses and 331 attributed
CrashInfo community diagnoses: 366 rules, all available in English and Spanish.

## Important behavior

- Last-file and last-library evidence is recorded live, with bounded chronological
  histories in the complete report.
- Game-local files are separated from Windows/background files; neither group is
  discarded. Windows account names are replaced with `<USERPROFILE>`.
- Duplicate ASI/CLEO detection reports the paths of every actively loaded copy.
- A new `Script suspended` entry appended to `cleo.log` produces its own Doctor
  report. Existing old CLEO errors are not replayed.
- Unknown evidence remains an unknown diagnosis. A crash is never treated as
  harmless merely because Doctor has not learned its signature yet.
- PECore's original crash dialog is not invoked by Doctor. Its original files and
  logs are preserved.

## Credits

Project Eagle-specific diagnoses are maintained by valkyrie from reproduced
reports. The embedded community list is CrashInfo by Junior_Djjr/MixMods and is
retained as a separate, attributed source. Project Eagle and S&SMP footer marks
are used for product identification; no open.mp mark is included.
