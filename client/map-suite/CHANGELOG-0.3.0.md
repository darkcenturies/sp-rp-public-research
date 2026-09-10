# Doctor Valkyrie 0.3.0 — changes since 0.2.9

## Diagnosis and evidence

- Restored and regression-tested the true last loaded file and library/module.
- Preserved duplicate active-component detection and both loaded paths.
- Added comprehensive, bounded file and library histories to saved reports.
- Separated Project Eagle/game-local activity from external/system activity.
- Redacted Windows user-profile directory names from displayed and saved reports.
- Clarified valid-but-unused/unregistered debug actor diagnoses.
- Added `0x00749B7B`: likely model/file requested before it was loaded, while
  explicitly warning that the last streamed file may be unrelated.
- Added the uncertain `0x49646550` item/model/texture family, preserving the
  competing asset, ENHI, mission-script, administrator/DPI and historical DEP
  hypotheses without presenting any unconfirmed explanation as fact.
- Added `0x006E3D9C` for animation/animation-group failures associated with the
  last performed action, with guidance to report the exact model and action.
- Added a direct missing-Project-Eagle diagnosis when `PECore.asi` is not loaded.
- Windowed Mode without winmode-nullfix now links to `https://sp-rp.com/`.
- Audio diagnoses again distinguish the Radio Patch from `PERadioAudioFix.asi`
  and continue recommending the ASI when appropriate, including when the Radio
  Patch is already installed.

## Interface

- Consolidated diagnosis, other findings and the complete report into one window.
- Made the comprehensive report the initial view instead of burying the raw log.
- Fixed the misleading empty Diagnosis heading and hid internal match-count prose
  from the player-facing views while retaining it in the saved audit report.
- Added offline English/Spanish switching for all player-facing text.
- Added version `0.3.0` to the title and copyright footer.
- Added Project Eagle and S&SMP footer marks, muted normally and colored on hover;
  labeled them as supported products, and omitted open.mp as requested. Doctor
  Valkyrie's copyright remains explicitly attributed to valkyrie.
- Made URLs supplied by diagnoses clickable.
- Fixed bottom-row clearance so controls are not clipped by the window edge.
- Retained normal minimize, maximize and close controls and responsive resizing.

## Crash ownership and CLEO

- Doctor now owns one comprehensive report and one visible crash window, avoiding
  a second competing PECore dialog while preserving the original log files.
- Added a separate `cleo.log` watcher for newly appended `Script suspended`
  errors, including the precise CLEO error context and bilingual explanation.

## Verification

- Added/updated regression coverage for last file, last library, duplicates,
  unused actors, BANKSLOT/no-base-game, both WidescreenFix paths, CLEO tailing,
  report paths/redaction, Spanish rule alignment, embedded resources, and the new
  crash addresses.
