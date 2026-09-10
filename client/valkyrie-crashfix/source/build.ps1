$ErrorActionPreference = 'Stop'
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source = Join-Path $PSScriptRoot 'ValkyrieCrashfix.cpp'
$out = Join-Path (Split-Path $PSScriptRoot -Parent) 'Valkyrie Crashfix.asi'
$cmd = '"' + $vcvars + '" x86 >nul && cl /nologo /std:c++17 /O2 /EHsc /MT /LD /D_CRT_SECURE_NO_WARNINGS "' + $source + '" /link /OUT:"' + $out + '" /INCREMENTAL:NO /MAP:"' + (Join-Path (Split-Path $PSScriptRoot -Parent) 'Valkyrie Crashfix.map') + '"'
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { throw "Build failed: $LASTEXITCODE" }
Get-Item -LiteralPath $out | Select-Object FullName,Length,LastWriteTime
