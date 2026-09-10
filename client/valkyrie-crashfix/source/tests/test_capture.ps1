$ErrorActionPreference='Stop'
$capture=Join-Path $PSScriptRoot '../../capture-evidence.ps1'
$testOutput=Join-Path $PWD 'capture-test'
$fixture=Start-Process -FilePath 'powershell.exe' -ArgumentList '-NoProfile','-Command','Start-Sleep -Seconds 60' -PassThru -WindowStyle Hidden
try {
    & $capture -ProcessId $fixture.Id -OutputDirectory $testOutput
    $latest=Get-ChildItem -LiteralPath $testOutput -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $manifest=Get-Content -LiteralPath (Join-Path $latest.FullName 'manifest.json') -Raw | ConvertFrom-Json
    if($manifest.capture_status -ne 'complete' -or $manifest.process_id -ne $fixture.Id){throw 'Capture metadata mismatch'}
    $bytes=[IO.File]::ReadAllBytes((Join-Path $latest.FullName 'process.dmp'))
    if($bytes.Length -lt 32 -or [Text.Encoding]::ASCII.GetString($bytes,0,4) -ne 'MDMP'){throw 'Invalid minidump header'}
    if($fixture.HasExited){throw 'Capture unexpectedly terminated target'}
    Write-Output 'PASS: external-process minidump and module metadata created; target remains running.'
} finally {
    if(-not $fixture.HasExited){$fixture.Kill();$fixture.WaitForExit()}
}
