# Valkyrie Crashfix 2.3.0-test

## Install

1. Close the game.
2. Back up your existing `valkyrie-crashfix.asi` outside the game folder.
3. Replace the existing file with `valkyrie-crashfix.asi` from this package. Keep only one crashfix ASI installed.
4. Launch the game. Check `Valkyrie Crashfix.log` for version 2.3.0-test and any skipped hooks.

To roll back, close the game and restore your backup.

## Changes from 2.2.2

Existing protections are retained. Added guards for:

- Missing actor health during the Blueberry Blues intro skip (VCF-045).
- Invalid train speed writes during loading (VCF-005/010).
- Invalid vehicle extras during contract/territory events (VCF-023/029).
- Invalid frame attachment during mission/teleport events (VCF-002).
- Helicopter searchlight accumulation (VCF-008): a conservative limit of two lights; additional lights are omitted.

Six reports receive targeted crash guards; one receives a partial mitigation. These guards cover specific invalid conditions, not every possible cause of those reports. Other mission freezes and crashes remain unresolved.

## Test status

Native and isolated machine-code tests passed, including existing guard regressions and 1,000 repeated searchlight enqueues. Gameplay reproduction is still pending. This is a test build.

SHA-256: `762f3cac2b80e1bd7e14eb059d4034c223dbdc1a11ce838da1abf49e5b37eaa8`

## Source and credits

Corresponding source and GPL-3.0 license: https://sp-rp.com/downloads/valkyrie-crashfix-2.3.0-test-source.7z

Credits: konradzimorski, Whitetiger, MTA:SA, Deji, 0x688, Ryosuke839 and bartekdvd. Original attribution and license notices are retained in the separate source archive.
