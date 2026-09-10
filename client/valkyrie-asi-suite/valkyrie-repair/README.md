# Valkyrie Repair 1.2

A standalone Windows repair app for GTA SA and Project Eagle. Scanning is
read-only. Every repair is individually approved and has an integrity-checked
backup or installation record for Undo. Close the game before applying repairs.

Includes Doctor Valkyrie 0.3.2, Crashfix 2.2.2 and Winmode Nullfix. Crashfix checks
recognize both spaced and hyphenated filenames. Missing copies can be installed;
known older copies are replaced at their existing path. Newer and unidentified
builds are not overwritten. Multiple candidate copies require review first.
Finding a file on disk does not prove that its hooks were installed at runtime.

Crashfix guidance links to https://sp-rp.com/#valkyrie-crashfix. Select a finding
to open its clickable download link. Disabling RadioEx is an optional diagnostic
action, not a prerequisite for installing Crashfix.

Build: run src/build.ps1 with .NET Framework 4.x installed. It produces the
shipping GUI and runs isolated self-tests in a separate executable. The test
executable uses generated temporary fixtures; its process-check bypass is never
compiled into the shipping application. The GUI retains the game-running check.

The embedded Doctor crash rules and third-party license are included in source.
Download: https://sp-rp.com/#valkyrie-repair
