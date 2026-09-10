# Public Valkyrie client mods

This folder contains Valkyrie Map and Doctor Valkyrie. Build with Visual Studio C++ Build Tools and the Windows SDK:

```powershell
./build.ps1 -Release
./build.ps1 -Release -OnlyTarget doctor-valkyrie
```

Outputs go to build/. The build does not install anything or connect to a server.

Map uses the earlier 0.2.0 source baseline, before the private 3D Radar development. It is not claimed to be the exact source of the later 0.2.1 binary download. Doctor is 0.3.2. The unreleased Valkyrie Radar implementation, its shared additions, routing component, research notes and tile pipeline are excluded.
