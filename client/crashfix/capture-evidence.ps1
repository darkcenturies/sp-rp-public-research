param(
    [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$ProcessId,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
# On-demand capture for a reproducible hang or pre-crash state. Does not attach
# a debugger, terminate the game, inject code, or change compatibility settings.
$ErrorActionPreference='Stop'
$targetProcess=Get-Process -Id $ProcessId
$targetStart=$targetProcess.StartTime.ToUniversalTime().ToString('o')
$targetName=$targetProcess.ProcessName
$targetPath=$targetProcess.Path
$taskOutput=[IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($taskOutput) | Out-Null
$captureName=('capture-{0}-{1}-{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'),$ProcessId,[guid]::NewGuid().ToString('N').Substring(0,8))
$captureRoot=Join-Path $taskOutput $captureName
[IO.Directory]::CreateDirectory($captureRoot) | Out-Null
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class ValkyrieDump {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
    [DllImport("dbghelp.dll", SetLastError=true)] public static extern bool MiniDumpWriteDump(IntPtr process,uint pid,IntPtr file,uint type,IntPtr exception,IntPtr streams,IntPtr callback);
}
'@
$metadata=[ordered]@{
    schema=1; captured_utc=(Get-Date).ToUniversalTime().ToString('o'); process_id=$ProcessId
    process_name=$targetName; executable=$targetPath; process_started_utc=$targetStart
    purpose='On-demand hang/pre-crash evidence; not a repaired crash or a first-fault exception dump'
    capture_status='pending'; modules=@(); logs_copied=@()
}
$processHandle=[ValkyrieDump]::OpenProcess(0x410,$false,[uint32]$ProcessId)
if($processHandle -eq [IntPtr]::Zero){throw "Cannot open target for read-only capture: Win32 error $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"}
$stream=$null
try {
    if((Get-Process -Id $ProcessId).StartTime.ToUniversalTime().ToString('o') -ne $targetStart){throw 'Target process changed before capture'}
    try {$metadata.modules=@($targetProcess.Modules | ForEach-Object { [ordered]@{name=$_.ModuleName;path=$_.FileName;base_address=('0x{0:X}' -f $_.BaseAddress.ToInt64());size=$_.ModuleMemorySize} })}
    catch {$metadata.module_inventory_error=$_.Exception.Message}
    $partial=Join-Path $captureRoot 'process.dmp.partial'
    $stream=[IO.File]::Open($partial,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
    # Thread info, unloaded modules and indirectly referenced memory. No full-memory flag.
    $ok=[ValkyrieDump]::MiniDumpWriteDump($processHandle,[uint32]$ProcessId,$stream.SafeFileHandle.DangerousGetHandle(),0x1060,[IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero)
    $dumpError=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
    $stream.Dispose();$stream=$null
    if(-not $ok){throw "MiniDumpWriteDump failed: Win32 error $dumpError"}
    Move-Item -LiteralPath $partial -Destination (Join-Path $captureRoot 'process.dmp')
    if($targetName -in @('gta_pe','gta_sa','gta-sa')){
        $gameRoot=Split-Path -Parent $targetPath
        foreach($logName in @('Valkyrie Crashfix.log','doctor-valkyrie.log','PECore.log')){
            $logPath=Join-Path $gameRoot $logName
            if(Test-Path -LiteralPath $logPath){
                try {Copy-Item -LiteralPath $logPath -Destination (Join-Path $captureRoot $logName);$metadata.logs_copied+=,$logName}
                catch {$metadata["copy_error_$logName"]=$_.Exception.Message}
            }
        }
    }
    $metadata.capture_status='complete'
} catch {
    $metadata.capture_status='failed';$metadata.error=$_.Exception.Message
    throw
} finally {
    if($stream){$stream.Dispose()}
    [ValkyrieDump]::CloseHandle($processHandle) | Out-Null
    $metadata | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $captureRoot 'manifest.json') -Encoding UTF8
}
Write-Output "Evidence saved: $captureRoot"
