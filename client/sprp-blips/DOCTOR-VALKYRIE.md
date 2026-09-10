# Doctor Valkyrie

Doctor Valkyrie is the Project Eagle crash reporter shipped as
`doctor-valkyrie.asi`. It owns one complete crash log and one native Windows
window, avoiding competing PECore dialogs. The main view opens on Diagnosis
and shows extra findings below the recommended action. The Full log view keeps
the comprehensive raw evidence. It embeds both Project Eagle
rules and the CrashInfo community list.

The `Español` button switches every player-facing control, diagnosis, other
finding, and readable complete-report heading to embedded UTF-8 Spanish. Raw
technical identifiers (paths, modules, registers, addresses and filenames)
remain unchanged so the evidence stays valid. Translation is offline at
runtime and sends no crash data anywhere. The ASI embeds 35 Project Eagle
translations plus 331 translated CrashInfo community rules: 366 English and
Spanish diagnoses in total. The window title remains simply `Project Eagle`;
the compact Valkyrie footer carries the exact build version.

URLs supplied by a diagnosis are clickable in every report view. The footer
uses the Project Eagle and S&SMP marks from the SP-RP website, muted until the
pointer hovers over them. Project Eagle opens its official site; S&SMP remains
an attributed mark without an invented link because no destination was supplied.
The separate `Supports:` / `Compatible con:` label makes clear that these are
supported games/mods, while the copyright explicitly attributes Doctor Valkyrie
itself to valkyrie.

Rule-match counts and source names remain in the saved technical report for
auditing, but are hidden from the player-facing window. Every scrollable view
is reset to its first line when opened so the primary diagnosis cannot appear
missing.

The complete report contains one populated `DIAGNOSIS` section. Its renderer
owns that heading; report assembly must not add a second heading before it,
because that makes the first section appear empty.

## Source layout

| Path | Purpose |
| --- | --- |
| `src/doctor.cpp` | crash filter, report writer, evidence tracking, environment checks |
| `src/doctorlist.*` | CrashInfo-compatible rule parser and matcher |
| `src/crashwindow.*` | native diagnosis window |
| `doctor-valkyrie.txt` | Project Eagle-owned crash rules |
| `res/CrashList.txt` | embedded CrashInfo community rules (attributed in the release README) |
| `res/doctor-*.bmp` | embedded known/unknown diagnostic banners |
| `res/doctor-{eagle,ssmp}*.bmp` | footer marks, including muted hover states |
| `res/*.es.txt` | offline Spanish prose aligned with both English rule lists |
| `tools/test-doctor-rules.cpp` | focused Project Eagle rule regressions |
| `tools/test-doctor-handler.cpp` | runtime crash-handler smoke test |
| `tools/test-cleo-watch.cpp` | CLEO log tailing, context, replay, and truncation regressions |
| `tools/test-report-paths.cpp` | game/external classification and username-redaction regressions |
| `tools/test-doctor-embedded.cpp` | validates rules embedded in the shipped ASI |
| `tools/doctor-selftest.cpp` | checks rule coverage against a folder of real logs |

## Evidence model

Doctor records two independent pieces of live crash evidence:

- **Last file loaded** comes from GTA's `CreateFileA/W` imports.
- **Last library loaded** comes from GTA's `LoadLibraryA/W` and
  `LoadLibraryExA/W` imports.

Only the main GTA executable is intercepted. Hooking third-party ASIs breaks
Mod Loader's caller translation, so the x86 wrappers save the argument and
tail-jump to the original import target without changing the caller return
address. The tracker intentionally ignores Doctor's own logs, preventing a
previous crash report from becoming misleading crash evidence.

When an unhandled exception occurs, Doctor writes a `doctor-valkyrie_*.log` in
the Project Eagle crash folder. It contains the exception, registers, stack,
backtrace, environment, diagnosis, and extra findings. The complete technical
report also lists every module loaded at failure, the latest 2,048 intercepted
file operations, and the latest 256 library-load operations in chronological
order. Fixed-size histories keep the tracker safe and bounded during long
sessions; the report explicitly says when older entries were omitted. Game-local
and external/system activity are separate sections, but neither is discarded.
The Windows account-directory prefix is written as `<USERPROFILE>` throughout
saved reports and the displayed log path so shared logs do not expose the
player's account name. Doctor intentionally
does not invoke PECore's crash handler, because doing so also opens PECore's
modal crash window.

A CLEO script suspension is not an unhandled exception, so Doctor monitors
new lines appended to `cleo.log` separately. When CLEO writes `Script
suspended`, Doctor preserves `cleo.log`, saves a `doctor-valkyrie_cleo_*.log`
containing the nearby CLEO error and an explanation, and presents it in the
same report window. Content already present when Doctor starts is ignored, so
old errors are never replayed. A frozen game with no CLEO suspension and no
Windows exception still requires a separate watchdog design with safeguards
against false positives.

Duplicate detection is deliberately separate: it snapshots the modules that
are actually loaded when the crash happens and reports duplicate ASI/CLEO
basenames. Do not replace it with filename tracking or weaken it while changing
the live tracker.

## Unused actor diagnostics

The debug-menu rule identifies a special case where a model name/ID can be
valid but is not registered for use in the active game session. The report
must not call that case merely an "invalid actor": it distinguishes an
invalid numeric ID from a valid-but-unregistered/unused model where the log
contains sufficient context. Known problematic selections are PSYCHO, LVPDM1,
and SFPDML1.

The `0x00749B7B` missing-model/script-function diagnosis was identified and
reproduced by the Project Eagle maintainer: loading the model before the
request fixed the crash. The rule remains explicit that the address can have
other causes and that the last streamed audio/file entry may be incidental.

`0x49646550` is intentionally documented as uncertain. Reports associate it
with an unavailable item/model/texture and with an old mission script error.
Doctor suggests asset/registration, ENHI, administrator/DPI, and historically
reported DEP checks without claiming any one of them is proven or advising a
system-wide DEP disable.

If `PECore.asi` is absent from the loaded-module snapshot, Doctor clearly says
that Project Eagle itself is missing and explains how to reinstall the complete
mod over a clean GTA SA 1.0 base. Windowed Mode users without winmode-nullfix
receive a direct download recommendation for https://sp-rp.com/.

## Building

Run `build.ps1` from this directory. It builds a 32-bit ASI because Project
Eagle/GTA SA is a 32-bit process. `build.ps1 -Install -GamePath <game folder>`
copies the built ASI into the selected game directory. The build embeds the
rules and header bitmaps, so no loose rule database is required at runtime.

## Regression checks

Before shipping a Doctor build:

1. Run `build.ps1` and confirm `build/doctor-valkyrie.asi` is created as x86.
2. Compile/run `tools/test-doctor-rules.cpp` with `src/doctorlist.cpp`; it
   covers last-file rule selection, last-library rule selection, unrelated
   library negatives, unused-actor evidence, both WidescreenFix evidence paths,
   BANKSLOT without an address, and duplicate loaded-component paths/counts.
3. Run `tools/test-doctor-embedded.cpp` against the built ASI to confirm both
   rule databases were embedded.
4. Run `tools/test-doctor-handler.cpp` in a disposable test folder and verify
   the written report contains the latest file and library lines.
5. Exercise a duplicate ASI/CLEO scenario and verify its findings remain
   present. Duplicate detection is a module snapshot, not a side effect of the
   live tracking hooks.

For real crash samples, run `doctor-selftest` against a log folder. Do not add
a rule solely because an unrelated module was loaded last; rule evidence must
be reproducible and specific.
