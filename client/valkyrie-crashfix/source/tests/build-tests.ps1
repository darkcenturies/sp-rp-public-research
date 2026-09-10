param([string]$OutputDirectory = $PWD)
$ErrorActionPreference='Stop'
$vswhere='C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products * -property installationPath
$vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$source=Join-Path $PSScriptRoot 'native_tests.cpp'
$out=Join-Path $OutputDirectory 'native-tests.exe'
$command='"'+$vcvars+'" x86 >nul && cl /nologo /std:c++17 /O2 /EHsc /MT /D_CRT_SECURE_NO_WARNINGS "'+$source+'" /Fe:"'+$out+'"'
cmd /c $command
if($LASTEXITCODE -ne 0){throw 'Native test build failed'}
& $out
if($LASTEXITCODE -ne 0){throw 'Native tests failed'}
