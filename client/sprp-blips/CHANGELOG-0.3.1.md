# Doctor Valkyrie 0.3.1 — changes since 0.3.0

## Interface

- The window title is now simply `Project Eagle`; the version remains visible
  in the compact `Valkyrie` footer.
- Diagnosis is always the opening screen.
- Other Findings now appears on the Diagnosis screen below the recommended
  action instead of requiring a separate tab.
- Renamed `Complete report` to `Full log` and `Copy report` to `Copy`.
- Reduced the Project Eagle and S&SMP marks and widened `Compatible con:` so the
  Spanish footer does not clip.
- Unknown crashes now say that Valkyrie lacks enough evidence to name the cause
  safely, without suggesting the crash might be harmless.

## Coverage audit

- Reverified that all 35 Project Eagle rules and all 331 CrashList community
  rules are embedded, with 366 aligned offline Spanish translations.
- Audited the supplied SA-MP CrashDetect source. Its Pawn VM interception,
  source-level AMX traces, callback timing, address-zero checks and runtime
  callback API are server-only and cannot execute in a GTA client ASI.
- Documented Valkyrie's client-side equivalents and the intentional server/client
  boundary in `CRASHDETECT-COVERAGE.md`.

## Verification

- Added regression coverage for the revised bilingual unknown-crash wording.
- Re-ran rule selection, Spanish alignment, embedded-resource, CLEO watcher and
  path/privacy tests against the hardened release binary.
