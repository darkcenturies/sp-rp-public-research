param(
    [Parameter(Mandatory = $true)][string] $ArchiveDirectory,
    [string] $OutputDirectory = (Join-Path $PSScriptRoot "build")
)

$ErrorActionPreference = "Stop"
$sevenZip = "C:\Program Files\7-Zip\7z.exe"
if (-not (Test-Path -LiteralPath $sevenZip)) {
    throw "7-Zip is required at $sevenZip"
}

function Get-Crc32([string] $Path) {
    [uint32] $crc = [uint32]::MaxValue
    [uint32] $polynomial = 3988292384
    $stream = [IO.File]::OpenRead($Path)
    try {
        $buffer = New-Object byte[] 16384
        while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
            for ($i = 0; $i -lt $count; $i++) {
                $crc = $crc -bxor $buffer[$i]
                for ($bit = 0; $bit -lt 8; $bit++) {
                    if (($crc -band 1) -ne 0) {
                        $crc = [uint32](($crc -shr 1) -bxor $polynomial)
                    } else {
                        $crc = [uint32]($crc -shr 1)
                    }
                }
            }
        }
    } finally {
        $stream.Dispose()
    }
    return [uint32]($crc -bxor [uint32]::MaxValue)
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$temporary = Join-Path ([IO.Path]::GetTempPath()) ("Valkyrie-map-" + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $archives = Get-ChildItem -LiteralPath $ArchiveDirectory -File |
        Where-Object { $_.Extension -in '.zip', '.7z' }
    foreach ($archive in $archives) {
        $extract = Join-Path $temporary $archive.BaseName
        New-Item -ItemType Directory -Path $extract | Out-Null
        & $sevenZip e -y "-o$extract" -- $archive.FullName `
            ProperRadarSA.txd "S&S\Cdimages\default\radar.img" | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Could not read $($archive.FullName)" }
        $atlas = Join-Path $extract "ProperRadarSA.txd"
        if (-not (Test-Path -LiteralPath $atlas)) { continue }
        $crc = Get-Crc32 $atlas
        $style = $archive.BaseName.ToLowerInvariant() -replace '\s+', '-'
        $name = "Valkyrie-map-overview-$style.txd"
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
            (Join-Path $PSScriptRoot "build-overview.ps1") `
            -Source $atlas -Output (Join-Path $OutputDirectory $name)
        if ($LASTEXITCODE -ne 0) { throw "Overview build failed for $($archive.Name)" }

        $radarImg = Join-Path $extract "radar.img"
        if (Test-Path -LiteralPath $radarImg) {
            $highName = "Valkyrie-map-overview-hi-{0:X8}.txd" -f $crc
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
                (Join-Path $PSScriptRoot "build-overview-hd.ps1") `
                -SourceImg $radarImg -Output (Join-Path $OutputDirectory $highName)
            if ($LASTEXITCODE -ne 0) {
                throw "HD overview build failed for $($archive.Name)"
            }
            $fullPrefix = Join-Path $OutputDirectory `
                ("Valkyrie-map-overview-full-{0:X8}" -f $crc)
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
                (Join-Path $PSScriptRoot "build-overview-full.ps1") `
                -SourceImg $radarImg -OutputPrefix $fullPrefix
            if ($LASTEXITCODE -ne 0) {
                throw "Full-detail overview build failed for $($archive.Name)"
            }
        }
    }
} finally {
    if ($temporary.StartsWith([IO.Path]::GetTempPath(), [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $temporary -Recurse -Force
    }
}
