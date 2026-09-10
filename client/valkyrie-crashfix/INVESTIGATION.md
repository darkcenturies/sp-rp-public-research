# Valkyrie Crashfix 2.2.1: correction and investigation

## 2.2.1 correction

Guard 01 in 2.2 incorrectly treated the signed specular-plugin offset at 0x8D12CC as a pointer. Normal small offsets triggered false positives and skipped valid vehicle specular rendering. The original test fixture also reversed the material pointer and offset, hiding the error. Version 2.2.1 corrects the guard and fixture, tests six small valid offsets, and rejects missing materials, an unregistered negative offset, address overflow, and missing specular data. The old high counter values must NOT be interpreted as prevented crashes.

2.2.1 passed 324 valid-path comparisons, 45 invalid cases, pool-state tests and native installer/RadioEx tests. It has NOT been installed into or gameplay-tested in the running session. The included 2.2-startup-validation.log is historical evidence for 2.2 only. The remaining audit below describes the underlying 2.2 work, with guard 01 and test counts corrected.

The recovery build 2.1.1 restored stability by disabling the imported guard group. Version 2.2 replaces that group with **27 individually checked guards**, while retaining the four RadioEx call hooks and the pool guard. Two old strategies were removed after investigation; they are not claimed as fixed.

## The reported crash

The supplied report places the fault at `Valkyrie Crashfix.asi+0x32C7`, reading address `0x0000005C` with `EDI=0`. Disassembly of the installed 2.1 binary shows a model-table read from `0xA9B0C8`, followed immediately by `mov edi,[edi+0x5C]`; the null test comes afterward. The installed ASI hash matched the 2.1 source package binary.

This is more than a missing null check. Fastman92's log reports an expanded 200,000-entry DFF model space and a relocated `CModelInfo::ms_modelInfoPtrs`. A read-only live snapshot confirmed that the original instruction at `0x5DF949` uses a different table address, while the old ASI hard-codes the stock address. The reported ECX value is 405. The live menu/startup snapshot has not loaded that model, so it does not establish the model object's contents at the instant of the original crash. It does confirm the incompatible table operand.

The new guard captures the actual operand and replays that exact live instruction. It checks the model before its vehicle-structure field, and uses the existing exception-safe epilogue when either is absent. Merely adding a null check to the old lookup would keep reading the wrong table and potentially skip valid work.

Doctor's audio diagnosis was a one-field rule match on file activity. `AUDIO\STREAMS\AMBIENCE` does not account for this demonstrated fault instruction. No audio files were replaced for this crash.

## Compatibility and installation

Every guard checks the supported instruction bytes and its recovery targets. The one supported variable model-table operand is captured from live code; the rest of that instruction and its continuation are checked. Altered or unsupported sites are skipped individually with an explicit reason in the log.

The live audit found SkyGFX's branch at `0x5D9A74` and a Fastman92 extension stub at `0x5E5815`. Guard 1 now runs after the rendering decision, at `0x5D9A7A`. Guard 16 intercepts the association getter call at `0x5E5810`, executes it once, checks its result, then continues through Fastman92's existing extension code. Neither mod's detour is overwritten.

The installer waits for installed late-loading mod modules and one second of stable hook-site bytes. It rechecks original bytes and recovery signatures with peer threads paused, refuses to patch when a peer instruction pointer is inside an overwrite region, and restores every written slot on transaction failure. Trampolines are executable/read-only after construction. The ASI is pinned for the process lifetime so patched code cannot jump into an unloaded module. Runtime monitoring reports later overwrites without fighting the other writer. Null-path counters are updated without calling logging or CRT code inside the game guard; a separate worker writes changes to the log.

These measures address the identified integration defects. They are not a proof that every third-party mod, future binary, use-after-free or concurrent memory mutation is safe.

## All 29 former entries

Numbering follows the imported source. Guard 19 was already excluded from the original 29 because of SkyGFX incompatibility and remains excluded.

| Old entry | Current hook address | Disposition | Investigation / resulting behavior |
|---|---|---|---|
| 1 | `005D9A7A` | Corrected in 2.2.1 | Missing vehicle specular-material data. Small plugin offsets are valid. Checks material, negative offset, address overflow and resulting data pointer. The 2.2 implementation was incorrect. |
| 2 | `006B18B0` | Rewritten | Missing vehicle collision data. No EBX clobber from suspension-line probe. |
| 3 | `00645FD9` | Rewritten | Missing car-door vehicle or handling. Sets output group to default and uses callee cleanup RET 8, not a CDECL abort. |
| 4 | `004F02D2` | Rewritten | Sound service division faults. Handles zero divisor and signed INT_MIN/-1 overflow. |
| 5 | `005DF949` | Rewritten | Relocated vehicle model table. Captures the live table operand, not 0xA9B0C8. Missing model/vehicle structure uses existing SEH epilogue. |
| 6 | `004D1750` | Rewritten | Null animation association.  |
| 7 | `00417BF8` | Rewritten | Null collision model.  |
| 8 | `0073485D` | Rewritten | Null 2D-effect object.  |
| 9 | `00738B64` | Rewritten | Null projectile. Reloads the projectile loop index before advancing; avoids stale EAX. |
| 10 | `005334FE` | Rewritten | Null transform vector. Returns initialized zero vector through the original output pointer. |
| 11 | `004D2C62` | Rewritten | Null animation association (second path).  |
| 12 | `004D41C5` | Rewritten | Null animation block.  |
| 13 | `004D464E` | Removed | The old fallback jumps past initialization and attempts to synthesize an association for callers that require a usable result. No general safe substitute was demonstrated. The native game path is left intact. |
| 14 | `004DD4B5` | Rewritten | Uninitialized audio event volumes. Restores the SEH chain established before this hook; old early return left FS:[0] dangling. |
| 15 | `0156CDAE` | Removed | The old timeout resumes streaming after a failed wait and clears the pending flag, without proof that the worker completed. This can expose incomplete data. Correct synchronization is retained. |
| 16 | `005E5810` | Rewritten | Missing first animation association. Calls the original association getter once, then checks its result BEFORE the existing Fastman92 extension stub. Real epilogue restores EDI/ESI and FS:[0]. |
| 17 | `007F120E` | Rewritten | Null RenderWare frame plugin owner.  |
| 18 | `004C7DAD` | Rewritten | Missing wheel frame.  |
| 20 | `0054F3B0` | Rewritten | Null placeable or matrix. Checks the matrix member as well as the object. |
| 21 | `00648EE0` | Rewritten | Null fall-out callback task. Checks actual task argument; no unpopulated association-lifetime registry. |
| 22 | `004CEF08` | Rewritten | Missing animation node source. Zeros only this node and returns to the real loop; no duplicated loop or changed valid behavior. |
| 23 | `006E3D10` | Rewritten | Door offset index bounds. The array has 7 vectors, not 16. Unsigned comparison rejects negative indices too. |
| 24 | `007F0DC8` | Rewritten | Null RenderWare child frame.  |
| 25 | `00646026` | Rewritten | Car-door callback without vehicle. Preserves EAX on valid path; original LEA/PUSH replayed exactly. |
| 26 | `00739FA0` | Rewritten | Invalid shot pedestrian list.  |
| 27 | `00637804` | Rewritten | Dying-in-car task without vehicle. Hooks the actual vehicle dereference after the original ped-state branch; preserves valid flags and ECX. |
| 28 | `0044A4FD` | Rewritten | Garage door without dummy.  |
| 29 | `004E022C` | Rewritten | Sound-bank slot bounds. Checks the signed slot against the live loader slot count at +0x0C and checks the slot array. Does not misclassify every low byte FF as -1. |
| Frame lookup | `004C53C0` | Rewritten | Null clump frame lookup. Retains normal null-result semantics; removes recursive nearest-frame guessing. |

The frame-lookup replacement protects a null clump. It deliberately preserves normal null-result semantics for a valid clump with no requested frame, rather than choosing a different frame. That is narrower coverage than the old heuristic and avoids silently attaching a component to an unrelated frame.

Pointer guards target missing/null/low-address data. They do not claim to validate every non-null object or repair a dangling allocation. The guards do not install a catch-all exception handler or skip arbitrary faulting instructions.

## Verification

`source/tests/test_guards.py` loads the actual built x86 ASI in Unicorn and executes its machine code. It compares each guard's valid path with the GTA instructions it replaces: general registers, flags, caller-visible stack, XMM registers, and x87 control. Scratch bytes in newly reserved, uninitialized stack locals are not treated as defined outputs.

- 324 valid-path comparisons across all 27 guards.
- 45 deliberate missing-data and boundary cases, including real game epilogues that restore FS:[0], ESI/EDI and the stack.
- Full 16-bit sound-slot regression cases; bounds use the live loader's slot count rather than a hard-coded count.
- The pool wrapper is tested with a helper that intentionally clobbers SSE/x87 state; its state restoration and null return are verified. Read-only pool headers are rejected because the allocator writes them.
- Native x86 installer tests inject a failure at every write position, check shared-page protection restoration, reject a competing patch, and reject a paused peer at a patch address while preserving its original suspension count.
- Native RadioEx tests exercise normal outputs, missing streams, error sentinels, zero duration, clamping, NaN/infinity, conversion failures and seek failures.

An x86 build emits MSVC C4733 for the inline restoration of FS:[0]. The code is restoring the already-established game exception chain, not registering a new exception handler. The restoration is exercised against the real epilogue layout in the emulator tests.

**Gameplay remains a required retest.** Isolated tests and a successful startup do not establish long-session stability or prove that every failure path occurs safely in a real mission. The existing 2.1.1 binary and log were backed up before installation.

## Reproducibility and evidence

- GTA executable SHA-256: `f115b49c5ac3a8fcedb00ee216730b2a6f573afe4908b2ac328a9cc2b9d0dcb3`.
- Built ASI SHA-256: `0377d83b3448e0275903a37eea1acc62ac3bc34636a23c2c9ff0a53171d45b46`.
- GTA Reversed reference revision: `0076f9017b4aeef7433957ac2bd8b6988e9df166`.
- `source/tests/audit.json`: exact supported bytes, recovery signatures, guard contracts and reviewed assembly.
- `source/tests/legacy-sites.json`: all original 29 hook entries.
- `source/tests/generate_guards.py`: reproducible guard/signature generation, restricted to the reviewed executable hash.
- `source/build.ps1`: MSVC x86 release build; only the rewritten source is compiled. The old imported source remains included for attribution and audit.
- `source/tests/build-tests.ps1`: build/run native tests from a scratch directory.
- `source/tests/test_guards.py`: run compiled-guard emulation after building.

## Primary references

- [Original Whitetiger crashfix source](https://github.com/Whitetigerswt/gtasa_crashfix), included locally with its GPL-3.0 license and original credits.
- [GTA Reversed: vehicle animation groups](https://github.com/gta-reversed/gta-reversed/blob/0076f9017b4aeef7433957ac2bd8b6988e9df166/source/game_sa/VehicleAnimGroupData.h): the door-offset array has seven vectors.
- [GTA Reversed: bank loader layout](https://github.com/gta-reversed/gta-reversed/blob/0076f9017b4aeef7433957ac2bd8b6988e9df166/source/game_sa/Audio/Loaders/AEBankLoader.h): slot count and slot-array layout.
- [GTA Reversed: ped behavior](https://github.com/gta-reversed/gta-reversed/blob/0076f9017b4aeef7433957ac2bd8b6988e9df166/source/game_sa/Entity/Ped/Ped.cpp): function mapping used alongside disassembly of the actual executable.

Original RadioEx discovery and v1: konradzimorski. Legacy guard research: Whitetiger and MTA contributors, with original source credits retained. The rewritten guards derive from GPL-3.0 code; corresponding source is supplied.

Startup result: the installed 2.2 binary logged all 32 hooks installed (27 rewritten GTA guards plus four RadioEx hooks and the pool guard). See startup-validation.log. The hidden inspection process was then closed for the user's gameplay test.
