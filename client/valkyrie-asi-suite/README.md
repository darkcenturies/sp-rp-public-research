# Public Valkyrie client mods

This folder is the canonical home for the public Valkyrie client family: Valkyrie Map, Doctor Valkyrie, Valkyrie Crashfix, and Valkyrie Repair.

Map and Doctor share `valkyrie-core` and use the suite build:

```powershell
./build.ps1 -Release
./build.ps1 -Release -OnlyTarget doctor-valkyrie
```

Outputs go to build/. The build does not install anything or connect to a server.

Crashfix and Repair remain independently buildable because they use different toolchains:

```powershell
./valkyrie-crashfix/src/build.ps1
./valkyrie-repair/src/build.ps1
```

Map uses the earlier 0.2.0 source baseline, before the private 3D Radar development. It is not claimed to be the exact source of the later 0.2.1 binary download. Doctor is 0.3.2. The unreleased Valkyrie Radar implementation, its shared additions, routing component, research notes and tile pipeline are excluded.
