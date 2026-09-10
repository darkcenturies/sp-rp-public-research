# Building ASI plugins

For DK22Pac's GTA development SDK, see [PLUGIN_SDK.md](PLUGIN_SDK.md). Its source is included at `third_party/plugin-sdk`; initialize the submodule before using it for new plugins.

The ASI builder is the C++ toolchain plus the checked-in PowerShell build recipes. These projects use Microsoft's `cl.exe` to compile C++ and link a 32-bit Windows DLL with an `.asi` filename. Doctor additionally uses the Windows resource compiler (`rc.exe`) to embed its diagnostic text and artwork. A separate ASI SDK is not required by the current public build recipes.

## Set up once

On Windows, install [Visual Studio Build Tools](https://visualstudio.microsoft.com/downloads/) or Visual Studio with **Desktop development with C++**, the MSVC x86/x64 tools, and a Windows SDK. Microsoft documents this setup in [Use the C++ Build Tools from the command line](https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line).

The repository contains our build recipes and source; it does not bundle Microsoft's compiler. `vswhere.exe` locates the installed toolchain, and `vcvarsall.bat x86` selects the 32-bit environment. Installing only the Visual C++ Redistributable does not supply this development toolchain.

Repair is a C# Windows application rather than an ASI. Its existing builder uses the .NET Framework 4.x compiler at `%WINDIR%/Microsoft.NET/Framework/v4.0.30319/csc.exe`.

## One entry point

Run these from the repository root in PowerShell:

```powershell
./build.ps1 -List
./build.ps1 -CheckEnvironment
./build.ps1 -Release
# Build only one project:
./build.ps1 -Target doctor -Release
```

Targets are `map`, `doctor`, `crashfix`, `repair`, or `all`. Environment checking locates the compiler entry points; the actual build verifies that the SDK and libraries are usable. It does not install software. No command above launches a mod, changes a game installation or deploys to the server.

Without `-Release`, Map and Doctor retain development diagnostics. Their release builds enable optimization and strip diagnostic logging. Crashfix and Repair retain their existing optimized build settings; `-Release` does not change those two recipes. Repair's build also runs its 26 isolated self-tests, without opening the repair GUI or repairing a real installation.

| Target | Underlying recipe | Output |
| --- | --- | --- |
| map | `client/sprp-blips/build.ps1 -OnlyTarget valkyrie-map` | `client/sprp-blips/build/valkyrie-map.asi` |
| doctor | `client/sprp-blips/build.ps1 -OnlyTarget doctor-valkyrie` | `client/sprp-blips/build/doctor-valkyrie.asi` |
| crashfix | `client/valkyrie-crashfix/source/build.ps1` | `client/valkyrie-crashfix/Valkyrie Crashfix.asi` |
| repair | `client/valkyrie-repair/source/build.ps1` | `client/valkyrie-repair/valkyrie-repair.exe` |

Intermediate files from the root wrapper go under `work/build-run/` or the component's ignored build directory. Public builds do not require the private gamemode or game executables. Running an ASI in a game is a separate step requiring the correct game version and loader.

## How the C++ recipes work

The Map/Doctor recipe sets C++17, `/LD` for a DLL, `/MT` for the static runtime, and the x86 environment. It generates `src/version.h`, compiles the target's source list, links system libraries and checks the PE machine field for `0x014c` (x86). Crashfix has its own C++17 x86 recipe and retained GPL-3.0 source. Edit the relevant recipe when adding a source file or resource; editing generated `version.h` is temporary.

For a new public ASI, document its source ownership, supported binary hashes, dependencies, build target and test plan. Use an existing public entry point as a reference. Keep heavy work outside DllMain and validate the target's bytes before applying hooks. Do not copy the private Radar implementation or assume the decompilation is directly compilable plugin code.

## Checks and common problems

- `python tools/check_public.py` verifies public scope, Python syntax and archive integrity.
- `client/valkyrie-crashfix/source/tests/build-tests.ps1` compiles/runs Crashfix's native tests; use an ignored working directory for its intermediate files.
- `client/valkyrie-crashfix/source/tests/requirements.txt` lists separate Python machine-code test dependencies. They are not prerequisites for ordinary ASI compilation.
- Missing `vswhere` or `vcvarsall`: install/modify the C++ workload.
- Missing `windows.h`, system libraries or `rc.exe`: check the Windows SDK in Visual Studio Installer.
- Wrong architecture: use the recipes' x86 environment; GTA SA's process is 32-bit even on 64-bit Windows.
- Build success does not verify an in-game hook against every executable revision. Record runtime validation separately.

See [the mod catalog](MODS.md) for exact source versions and remaining gaps, including the older Map baseline and missing Winmode Nullfix source.
