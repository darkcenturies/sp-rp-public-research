[CmdletBinding()]
param(
    [Parameter(Mandatory)][string] $GhidraHome,
    [Parameter(Mandatory)][string] $JdkHome,
    [Parameter(Mandatory)][string] $PyGhidraPython,
    [Parameter(Mandatory)][string] $TargetManifest,
    [string] $OutputRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'work\reverse-engineering\generated'),
    [string] $WorkRoot = (Join-Path (Split-Path $PSScriptRoot -Parent) 'work\reverse-engineering'),
    [string[]] $Only = @(),
    [int] $MaxCpu = 4
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Resolve-FullPath([string] $Path) {
    return [IO.Path]::GetFullPath($Path)
}

function Assert-ChildPath([string] $Parent, [string] $Child) {
    $parentFull = (Resolve-FullPath $Parent).TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
    $childFull = Resolve-FullPath $Child
    if (-not $childFull.StartsWith($parentFull, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to use path outside $parentFull`: $childFull"
    }
    return $childFull
}

function Convert-ToWslPath([string] $WindowsPath) {
    $full = Resolve-FullPath $WindowsPath
    if ($full -notmatch '^([A-Za-z]):\\(.*)$') {
        throw "Expected a drive-qualified Windows path, got: $full"
    }
    $drive = $Matches[1].ToLowerInvariant()
    $rest = $Matches[2] -replace '\\', '/'
    return "/mnt/$drive/$rest"
}

function Get-TargetInput($Target, [string] $InputRoot) {
    if ($Target.source.type -ne 'local') { throw 'Use source.type=local with an absolute path to your own input.' }
    $inputPath = [string] $Target.source.path
    if (-not [IO.Path]::IsPathFullyQualified($inputPath)) { throw 'Input paths must be absolute.' }
    return Resolve-FullPath $inputPath
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$outputFull = Resolve-FullPath $OutputRoot
$workFull = Resolve-FullPath $WorkRoot
$inputRoot = Assert-ChildPath $workFull (Join-Path $workFull 'inputs')
$projectRoot = Assert-ChildPath $workFull (Join-Path $workFull 'projects')
$logRoot = Assert-ChildPath $workFull (Join-Path $workFull 'logs')

New-Item -ItemType Directory -Force -Path $outputFull, $inputRoot, $projectRoot, $logRoot | Out-Null

$headless = Get-ChildItem -LiteralPath $GhidraHome -Filter 'analyzeHeadless.bat' -File -Recurse |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $headless) {
    throw "analyzeHeadless.bat was not found under $GhidraHome"
}
if (-not (Test-Path -LiteralPath (Join-Path $JdkHome 'bin\javac.exe'))) {
    throw "A 64-bit JDK 21 was not found under $JdkHome"
}
if (-not (Test-Path -LiteralPath $PyGhidraPython)) {
    throw "The PyGhidra Python environment was not found: $PyGhidraPython"
}

$exportScript = Join-Path $PSScriptRoot 'ghidra_scripts\ExportDecompiled.py'
if (-not (Test-Path -LiteralPath $exportScript)) {
    throw "Missing Ghidra export script: $exportScript"
}

$manifest = Get-Content -LiteralPath $TargetManifest -Raw | ConvertFrom-Json
$targets = @($manifest.targets)
if ($Only.Count -gt 0) {
    $wanted = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $Only | ForEach-Object { [void] $wanted.Add($_) }
    $targets = @($targets | Where-Object { $wanted.Contains([string] $_.id) })
    if ($targets.Count -ne $wanted.Count) {
        $found = @($targets | ForEach-Object { [string] $_.id })
        $missing = @($Only | Where-Object { $_ -notin $found })
        throw "Unknown target id(s): $($missing -join ', ')"
    }
}

$results = @()
foreach ($target in $targets) {
    $id = [string] $target.id
    Write-Host "[decompile] $id" -ForegroundColor Green

    $targetInput = Get-TargetInput $target $inputRoot
    if (-not (Test-Path -LiteralPath $targetInput -PathType Leaf)) {
        throw "Missing input for $id`: $targetInput"
    }

    $file = Get-Item -LiteralPath $targetInput
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetInput).Hash.ToLowerInvariant()
    if ($file.Length -ne [long] $target.bytes) {
        throw "$id size mismatch: expected $($target.bytes), got $($file.Length)"
    }
    if ($hash -ne ([string] $target.sha256).ToLowerInvariant()) {
        throw "$id SHA-256 mismatch: expected $($target.sha256), got $hash"
    }

    $targetOutput = Assert-ChildPath $outputFull (Join-Path $outputFull $id)
    if (Test-Path -LiteralPath $targetOutput) {
        throw "Output already exists: $targetOutput. Choose a fresh OutputRoot to preserve earlier evidence."
    }
    New-Item -ItemType Directory -Force -Path $targetOutput | Out-Null

    $projectName = 're-' + ($id -replace '[^A-Za-z0-9_.-]', '-')
    $log = Assert-ChildPath $logRoot (Join-Path $logRoot ($id + '.log'))
    $scriptPath = Split-Path $exportScript -Parent

    $oldJavaHome = $env:JAVA_HOME
    $env:JAVA_HOME = $JdkHome
    & $PyGhidraPython -m pyghidra.ghidra_launch `
        --install-dir $GhidraHome `
        ghidra.app.util.headless.AnalyzeHeadless `
        $projectRoot $projectName `
        -import $targetInput `
        -analysisTimeoutPerFile 3600 `
        -max-cpu $MaxCpu `
        -scriptPath $scriptPath `
        -postScript 'ExportDecompiled.py' $targetOutput `
        -deleteProject *> $log
    $ghidraExit = $LASTEXITCODE
    $env:JAVA_HOME = $oldJavaHome
    if ($ghidraExit -ne 0) {
        Get-Content -LiteralPath $log -Tail 80
        throw "Ghidra failed for $id (exit $ghidraExit)"
    }

    $metadataPath = Join-Path $targetOutput 'metadata.json'
    if (-not (Test-Path -LiteralPath $metadataPath)) {
        Get-Content -LiteralPath $log -Tail 80
        throw "Ghidra did not produce metadata for $id"
    }
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    if (([string] $metadata.sha256).ToLowerInvariant() -ne $hash) {
        throw "Exported metadata hash does not match $id"
    }
    if ([int] $metadata.functions_internal -lt 1) {
        throw "No internal functions were recovered for $id"
    }

    $results += [ordered]@{
        id = $id
        owner = [string] $target.owner
        role = [string] $target.role
        bytes = $file.Length
        sha256 = $hash
        functions_total = [int] $metadata.functions_total
        functions_internal = [int] $metadata.functions_internal
        functions_decompiled = [int] $metadata.functions_decompiled
        functions_failed = [int] $metadata.functions_failed
        strings = [int] $metadata.strings
        symbols = [int] $metadata.symbols
    }
    Write-Host "  functions $($metadata.functions_decompiled)/$($metadata.functions_internal), failed $($metadata.functions_failed)"
}

$indexedResults = @()
foreach ($target in @($manifest.targets)) {
    $id = [string] $target.id
    $metadataPath = Join-Path (Join-Path $outputFull $id) 'metadata.json'
    if (-not (Test-Path -LiteralPath $metadataPath)) {
        continue
    }
    $metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
    if (([string] $metadata.sha256).ToLowerInvariant() -ne ([string] $target.sha256).ToLowerInvariant()) {
        throw "Existing completed export has the wrong hash: $id"
    }
    $indexedResults += [ordered]@{
        id = $id
        owner = [string] $target.owner
        role = [string] $target.role
        bytes = [long] $target.bytes
        sha256 = ([string] $target.sha256).ToLowerInvariant()
        functions_total = [int] $metadata.functions_total
        functions_internal = [int] $metadata.functions_internal
        functions_decompiled = [int] $metadata.functions_decompiled
        functions_failed = [int] $metadata.functions_failed
        strings = [int] $metadata.strings
        symbols = [int] $metadata.symbols
    }
}

$index = [ordered]@{
    schema = 1
    ghidra = '12.1.3'
    exporter = 'deploy/ghidra_scripts/ExportDecompiled.py'
    targets = $indexedResults
}
$index | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputFull 'index.json') -Encoding utf8NoBOM

Write-Host "[decompile] complete: $($results.Count) target(s) -> $outputFull" -ForegroundColor Green
