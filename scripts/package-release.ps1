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
$openXrLoaderSource = Join-Path $repositoryRoot 'external/openxr-loader'
$openXrLoaderBuild = Join-Path $repositoryRoot 'build/openxr-loader'
$openXrLoader = Join-Path $openXrLoaderBuild `
    'src/loader/Release/openxr_loader.dll'
$exampleIni = Join-Path $repositoryRoot 'config/witcher3vr.example.ini'
$readme = Join-Path $repositoryRoot 'README.md'
$license = Join-Path $repositoryRoot 'LICENSE'
$thirdPartyNotices = Join-Path $repositoryRoot 'THIRD_PARTY_NOTICES.md'
$stateBridgeRoot = Join-Path $repositoryRoot 'support/modWitcher3VRStateBridge'
$stateBridgeScript = Join-Path $stateBridgeRoot `
    'content/scripts/local/witcher3vr/first_person_state_bridge.ws'
$hudEditorRoot = Join-Path $repositoryRoot 'support/modWitcher3VRHUDEditor'
$hudEditorScript = Join-Path $hudEditorRoot `
    'content/scripts/local/witcher3vr_hud_editor/hud_editor.ws'
$hudEditorXml = Join-Path $repositoryRoot `
    'support/modWitcher3VRHUDEditor.xml'
$movementDlcRoot = Join-Path $repositoryRoot 'support/dlcmovementinputfix'
$movementDlcBundle = Join-Path $movementDlcRoot 'content/blob0.bundle'
$movementDlcMetadata = Join-Path $movementDlcRoot 'content/metadata.store'

if (-not (Test-Path -LiteralPath (Join-Path $openXrLoaderSource 'CMakeLists.txt') `
        -PathType Leaf)) {
    throw 'Missing OpenXR loader source. Run scripts/setup-dependencies.ps1 first.'
}
if (-not (Test-Path -LiteralPath $openXrLoader -PathType Leaf)) {
    & cmake -S $openXrLoaderSource -B $openXrLoaderBuild -A x64 `
        -DDYNAMIC_LOADER=ON
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to configure the pinned OpenXR loader.'
    }
    & cmake --build $openXrLoaderBuild --config Release --target openxr_loader
    if ($LASTEXITCODE -ne 0) {
        throw 'Failed to build the pinned OpenXR loader.'
    }
}

foreach ($requiredFile in @(
        $releaseDll,
        $launcher,
        $openXrLoader,
        $exampleIni,
        $readme,
        $license,
        $thirdPartyNotices,
        $stateBridgeScript,
        $hudEditorScript,
        $hudEditorXml,
        $movementDlcBundle,
        $movementDlcMetadata)) {
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
        $trackedChanges = git -C $repositoryRoot status --porcelain `
            --untracked-files=no
        if ($trackedChanges) {
            $manifestLines += 'Worktree=dirty'
        } else {
            $manifestLines += 'Worktree=clean'
        }
    }

    # The archive mirrors the game root. Users extract it directly into
    # "The Witcher 3" instead of manually relocating the DLL, launcher or mod.
    $releaseStage = Join-Path $stagingRoot 'game-root'
    New-Item -ItemType Directory -Path $releaseStage | Out-Null

    $binaryStage = Join-Path $releaseStage 'bin/x64_dx12'
    New-Item -ItemType Directory -Path $binaryStage | Out-Null
    Copy-Item -LiteralPath $releaseDll `
        -Destination (Join-Path $binaryStage 'dxgi.dll')
    Copy-Item -LiteralPath $launcher -Destination $binaryStage
    Copy-Item -LiteralPath $openXrLoader -Destination $binaryStage

    $modsStage = Join-Path $releaseStage 'mods'
    New-Item -ItemType Directory -Path $modsStage | Out-Null
    Copy-Item -LiteralPath $stateBridgeRoot `
        -Destination (Join-Path $modsStage 'modWitcher3VRStateBridge') -Recurse
    Copy-Item -LiteralPath $hudEditorRoot `
        -Destination (Join-Path $modsStage 'modWitcher3VRHUDEditor') -Recurse

    $configMatrixStage = Join-Path $releaseStage `
        'bin/config/r4game/user_config_matrix/pc'
    New-Item -ItemType Directory -Path $configMatrixStage -Force | Out-Null
    Copy-Item -LiteralPath $hudEditorXml -Destination $configMatrixStage

    $dlcStage = Join-Path $releaseStage 'dlc'
    New-Item -ItemType Directory -Path $dlcStage | Out-Null
    Copy-Item -LiteralPath $movementDlcRoot `
        -Destination (Join-Path $dlcStage 'dlcmovementinputfix') -Recurse

    $documentationStage = Join-Path $releaseStage 'Witcher3VR'
    $configStage = Join-Path $documentationStage 'config'
    New-Item -ItemType Directory -Path $configStage -Force | Out-Null
    Copy-Item -LiteralPath $exampleIni `
        -Destination (Join-Path $configStage 'witcher3vr.example.ini')
    Copy-Item -LiteralPath $readme -Destination $documentationStage
    Copy-Item -LiteralPath $license -Destination $documentationStage
    Copy-Item -LiteralPath $thirdPartyNotices -Destination $documentationStage
    Set-Content -LiteralPath (Join-Path $documentationStage 'BUILD.txt') `
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
    $openXrLoaderHash =
        (Get-FileHash -LiteralPath $openXrLoader -Algorithm SHA256).Hash
    $movementDlcBundleHash =
        (Get-FileHash -LiteralPath $movementDlcBundle -Algorithm SHA256).Hash
    $movementDlcMetadataHash =
        (Get-FileHash -LiteralPath $movementDlcMetadata -Algorithm SHA256).Hash
    $hudEditorScriptHash =
        (Get-FileHash -LiteralPath $hudEditorScript -Algorithm SHA256).Hash
    $hudEditorXmlHash =
        (Get-FileHash -LiteralPath $hudEditorXml -Algorithm SHA256).Hash
    $archiveHash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash
    $manifestLines += "dll.sha256=$dllHash"
    $manifestLines += "launcher.sha256=$launcherHash"
    $manifestLines += "openxr_loader.sha256=$openXrLoaderHash"
    $manifestLines += "movement_dlc_bundle.sha256=$movementDlcBundleHash"
    $manifestLines += "movement_dlc_metadata.sha256=$movementDlcMetadataHash"
    $manifestLines += "hud_editor_script.sha256=$hudEditorScriptHash"
    $manifestLines += "hud_editor_xml.sha256=$hudEditorXmlHash"
    $manifestLines += "archive.sha256=$archiveHash"

    $manifestPath = Join-Path $OutputDirectory "Witcher3VR-$normalizedVersion-SHA256.txt"
    Set-Content -LiteralPath $manifestPath -Value $manifestLines -Encoding utf8
    Write-Host "Release packages written to $OutputDirectory"
} finally {
    if (Test-Path -LiteralPath $stagingRoot) {
        Remove-Item -LiteralPath $stagingRoot -Recurse -Force
    }
}
