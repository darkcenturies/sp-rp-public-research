$ErrorActionPreference = 'Stop'
$source = $PSScriptRoot
$root = Split-Path $source -Parent
$csc = 'C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.exe'
$common = @(
    '/nologo','/optimize+','/platform:x86',
    '/reference:System.dll','/reference:System.Core.dll',
    '/reference:System.Drawing.dll','/reference:System.Windows.Forms.dll',
    ('/win32icon:' + (Join-Path $source 'repair-icon.ico')),
    ('/win32manifest:' + (Join-Path $source 'app.manifest'))
)
$names = @('banner-thinking.png','banner-yay.png','banner-stop.png','banner-unsure.png','doctor-eagle.bmp','doctor-ssmp.bmp','sprp-logo.png','doctor-valkyrie.asi','doctor-valkyrie.txt','CrashList.txt','valkyrie-crashfix.asi','winmode-nullfix.asi')
$resources = $names | ForEach-Object { '/resource:' + (Join-Path $source $_) + ',' + $_ }
$sources = @('Program.cs','Engine.cs','CrashfixSupport.cs','MainForm.cs','SelfTests.cs') | ForEach-Object { Join-Path $source $_ }
$output = Join-Path $root 'valkyrie-repair.exe'
& $csc @common '/target:winexe' ('/out:' + $output) @resources @sources
if ($LASTEXITCODE -ne 0) { throw 'Valkyrie Repair build failed' }

$test = Join-Path $env:TEMP 'valkyrie-repair-self-test.exe'
& $csc @common '/define:VALKYRIE_TEST' '/target:exe' ('/out:' + $test) @resources @sources
if ($LASTEXITCODE -ne 0) { throw 'Valkyrie Repair test build failed' }
& $test --self-test
if ($LASTEXITCODE -ne 0) { throw 'Valkyrie Repair self-tests failed' }
Get-Item $output | Select-Object FullName,Length,LastWriteTime
