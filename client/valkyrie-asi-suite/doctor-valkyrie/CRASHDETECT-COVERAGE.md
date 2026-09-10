# CrashList and CrashDetect coverage

Doctor Valkyrie is a GTA San Andreas **client** crash reporter. CrashDetect is
a SA-MP **server** plugin that instruments the Pawn/AMX virtual machine. They
overlap in purpose—better diagnostics—but do not run in the same process and
cannot provide identical APIs.

## CrashList

Valkyrie embeds the full supplied CrashList database: 331 of 331 parsed rules.
It preserves multi-cause entries, problem/solution prose, addresses, files,
libraries and attribution. Together with 35 Project Eagle rules, the shipped
ASI contains 366 English diagnoses and 366 aligned offline Spanish diagnoses.
Automated tests fail if those counts or translations drift.

## CrashDetect comparison

| CrashDetect capability | Valkyrie status |
| --- | --- |
| Native process exception details | Client equivalent: exception address/code, registers, stack and native backtrace |
| Separate diagnostic log | Supported; original PE/CLEO logs are preserved |
| Pawn source filenames, lines, symbols and parameters | Server-only; requires AMX debug metadata inside the SA-MP server |
| Native/public/Pawn function tracing and regex filters | Server-only AMX instrumentation |
| Long callback/hang timing | Server-only callback timing; not equivalent to detecting a frozen game |
| Address-zero Pawn writes | Server-only AMX memory check |
| `OnRuntimeError`, `PrintBacktrace`, `GetBacktrace` Pawn API | Server-only scripting API |
| Client CLEO runtime errors | Valkyrie equivalent: watches new `Script suspended` records in `cleo.log` |
| Last opened file/library and bounded histories | Valkyrie-specific client evidence |
| Duplicate active ASI/module detection | Valkyrie-specific client evidence |
| Bilingual player-facing diagnosis | Valkyrie-specific, fully offline |

CrashDetect should remain installed on development/test SA-MP servers when Pawn
diagnostics are needed. Bundling its server VM hooks into Doctor Valkyrie would
not add client coverage; it would be dead code in `gta_sa.exe`. A future server
log viewer could consume CrashDetect output, but that would be a separate tool
and must not be presented as client crash detection.
