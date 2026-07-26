[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^[Vv][0-9]+$')]
    [string] $Version,

    [string] $OutputDirectory = (Join-Path (Split-Path -Parent $PSScriptRoot) 'dist')
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$repositoryPrefix = $repositoryRoot.TrimEnd('\') + '\'
if (-not $resolvedOutput.StartsWith(
        $repositoryPrefix,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'OutputDirectory must be inside the repository workspace.'
}
$OutputDirectory = $resolvedOutput
$normalizedVersion = $Version.ToUpperInvariant()
$releaseDll = Join-Path $repositoryRoot 'build/release/Release/dxgi.dll'
$launcher = Join-Path $repositoryRoot 'build/release/launcher/Release/Witcher3VRLauncher.exe'
$exampleIni = Join-Path $repositoryRoot 'config/witcher3vr.example.ini'
$readme = Join-Path $repositoryRoot 'README.md'
$license = Join-Path $repositoryRoot 'LICENSE'
$thirdPartyNotices = Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md'
$stateBridgeRoot = Join-Path $repositoryRoot 'support/modWitcher3VRStateBridge'
$stateBridgeScript = Join-Path $stateBridgeRoot `
    'content/scripts/local/witcher3vr/first_person_state_bridge.ws'

foreach ($requiredFile in @(
        $releaseDll,
        $launcher,
        $exampleIni,
        $readme,
        $license,
        $thirdPartyNotices,
        $stateBridgeScript)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Missing release input: $requiredFile"
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$stagingRoot = Join-Path $OutputDirectory '.staging'
if (Test-Path -LiteralPath $stagingRoot) {
    Remove-Item -LiteralPath $stagingRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingRoot | Out-Null

try {
    $manifestLines = @("Version=$normalizedVersion")
    $commit = git -C $repositoryRoot rev-parse --verify HEAD 2>$null
    if ($LASTEXITCODE -eq 0) {
        $manifestLines += "Commit=$commit"
    }

    $releaseStage = Join-Path $stagingRoot 'release'
    New-Item -ItemType Directory -Path $releaseStage | Out-Null
    Copy-Item -LiteralPath $releaseDll -Destination (Join-Path $releaseStage 'dxgi.dll')
    Copy-Item -LiteralPath $launcher -Destination $releaseStage
    Copy-Item -LiteralPath $exampleIni -Destination (Join-Path $releaseStage 'witcher3vr.example.ini')
    Copy-Item -LiteralPath $readme -Destination $releaseStage
    Copy-Item -LiteralPath $license -Destination $releaseStage
    Copy-Item -LiteralPath $thirdPartyNotices -Destination $releaseStage
    $modsStage = Join-Path $releaseStage 'mods'
    New-Item -ItemType Directory -Path $modsStage | Out-Null
    Copy-Item -LiteralPath $stateBridgeRoot `
        -Destination (Join-Path $modsStage 'modWitcher3VRStateBridge') -Recurse
    Set-Content -LiteralPath (Join-Path $releaseStage 'BUILD.txt') `
        -Value @(
            $normalizedVersion,
            'One optimized DLL for gaming and diagnostics.',
            'Use Diagnostic Logging in the launcher when support logs are needed.'
        ) -Encoding utf8

    $archivePath = Join-Path $OutputDirectory "Witcher3VR-$normalizedVersion.zip"
    if (Test-Path -LiteralPath $archivePath) {
        Remove-Item -LiteralPath $archivePath -Force
    }
    Compress-Archive -Path (Join-Path $releaseStage '*') -DestinationPath $archivePath

    $dllHash = (Get-FileHash -LiteralPath $releaseDll -Algorithm SHA256).Hash
    $launcherHash = (Get-FileHash -LiteralPath $launcher -Algorithm SHA256).Hash
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
    $manifestLines += "dll.sha256=$dllHash"
    $manifestLines += "launcher.sha256=$launcherHash"
    $manifestLines += "archive.sha256=$archiveHash"

    $manifestPath = Join-Path $OutputDirectory "Witcher3VR-$normalizedVersion-SHA256.txt"
    Set-Content -LiteralPath $manifestPath -Value $manifestLines -Encoding utf8
    Write-Host "Release packages written to $OutputDirectory"
} finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
