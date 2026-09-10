# Public mod source catalog

Use the root `build.ps1` to build these projects from one entry point. See [ASI builder and compiler setup](BUILDING.md); AI agents should start with [AGENT_START.md](AGENT_START.md).

| Mod | Source snapshot | Build |
| --- | --- | --- |
| Valkyrie Map | Earlier 0.2.0 baseline before private Radar development; later public download is 0.2.1-test | client/sprp-blips/build.ps1 -Release -OnlyTarget valkyrie-map |
| Doctor Valkyrie | 0.3.2 | client/sprp-blips/build.ps1 -Release -OnlyTarget doctor-valkyrie |
| Valkyrie Crashfix | 2.3.0-test, GPL-3.0 | client/valkyrie-crashfix/source/build.ps1 |
| Valkyrie Repair | 1.2 | client/valkyrie-repair/source/build.ps1 |
| Winmode Nullfix | Previously released binary embedded in Repair; standalone package contains binary and README only | Source not found in the inspected repository or public package. |

Run the PowerShell scripts on Windows. The ASIs require Visual Studio C++ Build Tools with the Windows SDK (x86 target). Repair uses .NET Framework 4.x; its build runs isolated self-tests without launching the GUI. Crashfix's native tests are in source/tests/build-tests.ps1; machine-code tests have their own requirements.txt.

Download records were checked against the project's mod release inventory on 2026-09-10. This export includes the earlier Map baseline and the current Doctor/Crashfix/Repair source, not every historical source version. Unreleased Valkyrie Radar work is excluded. Repair retains its shipped Doctor 0.3.2, Crashfix 2.2.2 and Nullfix payloads so its original resources remain buildable; the newer standalone Crashfix source is 2.3.0-test.

## Known gaps

- Winmode Nullfix source needs to be located and added; its binary is not a substitute for source.
- Map 0.2.1 exact corresponding source has not been isolated; the public tree retains its earlier 0.2.0 baseline without private Radar additions.
- No unreleased Radar work, game assets, private gamemode or production configuration are included.
- Existing research notes refer to some screenshots and test fixtures that are not public artifacts.
- Compilation and isolated tests do not establish real-game compatibility. Game testing requires a matching local installation and remains separate.

Player downloads: https://sp-rp.com/ . Report source gaps or build problems through a pull request or issue.
