param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [string]$BuildDirectory = "build",
    [string]$OutputDirectory = "build/release-assets"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$versionPattern = '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$'
if ($Version -cnotmatch $versionPattern) {
    throw "Version '$Version' is not a supported semantic version."
}
if ($Matches[4]) {
    foreach ($identifier in $Matches[4].Split('.')) {
        if ($identifier -cmatch '^[0-9]+$' -and $identifier.Length -gt 1 -and $identifier.StartsWith('0')) {
            throw "Numeric prerelease identifiers cannot contain leading zeroes."
        }
    }
}

function Resolve-RepoPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $Path))
}

$buildRoot = Resolve-RepoPath $BuildDirectory
$outputRoot = Resolve-RepoPath $OutputDirectory
$binaryDirectory = Join-Path $buildRoot "bin/Release"
$gameExecutable = Join-Path $binaryDirectory "MinecraftCopy.exe"
$launcherExecutable = Join-Path $binaryDirectory "MinecraftCopyLauncher.exe"
$textureAtlas = Join-Path $binaryDirectory "assets/textureatlas.png"

foreach ($requiredFile in @($gameExecutable, $launcherExecutable, $textureAtlas, (Join-Path $repoRoot "LICENSE"), (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"))) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required release file is missing: $requiredFile"
    }
}
if (-not (Test-Path -LiteralPath (Join-Path $repoRoot "launcher/ui/index.html") -PathType Leaf)) {
    throw "The launcher UI has not been built or is missing from the source tree."
}

$reportedVersion = (& $gameExecutable --version | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $reportedVersion -cne $Version) {
    throw "The game executable reports '$reportedVersion'; expected '$Version'."
}

$outputParent = Split-Path -Parent $outputRoot
if (-not (Test-Path -LiteralPath $outputParent -PathType Container)) {
    New-Item -ItemType Directory -Path $outputParent -Force | Out-Null
}
$releaseDirectory = Join-Path $outputRoot "v$Version"
if (Test-Path -LiteralPath $releaseDirectory) {
    throw "Release output already exists: $releaseDirectory. Move or remove it before packaging again."
}
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

$stagingDirectory = Join-Path $outputRoot (".staging-v{0}-{1}" -f $Version, [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $stagingDirectory | Out-Null

try {
    Copy-Item -LiteralPath $gameExecutable -Destination (Join-Path $stagingDirectory "MinecraftCopy.exe")
    Copy-Item -LiteralPath $textureAtlas -Destination (Join-Path $stagingDirectory "textureatlas.png")

    $launcherStage = Join-Path $stagingDirectory "launcher-package"
    New-Item -ItemType Directory -Path (Join-Path $launcherStage "launcher") -Force | Out-Null
    Copy-Item -LiteralPath $launcherExecutable -Destination (Join-Path $launcherStage "MinecraftCopyLauncher.exe")
    Copy-Item -LiteralPath (Join-Path $repoRoot "launcher/ui") -Destination (Join-Path $launcherStage "launcher/ui") -Recurse
    Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $launcherStage
    Copy-Item -LiteralPath (Join-Path $repoRoot "THIRD_PARTY_NOTICES.md") -Destination $launcherStage
    Copy-Item -LiteralPath (Join-Path $repoRoot "third_party/licenses") -Destination (Join-Path $launcherStage "third_party/licenses") -Recurse

    $installInstructions = @"
MinecraftCopy Launcher for Windows x64

Extract the complete ZIP file to a folder, then run MinecraftCopyLauncher.exe.
Keep the launcher/ui folder beside the executable. On first launch, choose Install
Game; later launches can check GitHub for a newer game version and install it.

The launcher requires the Microsoft Edge WebView2 Evergreen Runtime and an internet
connection. The MinecraftCopy GitHub repository must be public for downloads to work.
The launcher updates the game; replace the launcher package manually when a newer
launcher is published.
"@
    [System.IO.File]::WriteAllText((Join-Path $launcherStage "INSTALL.txt"), $installInstructions.Trim() + "`r`n", [System.Text.UTF8Encoding]::new($false))

    $launcherZip = Join-Path $stagingDirectory "MinecraftCopyLauncher-windows-x64.zip"
    Compress-Archive -Path (Join-Path $launcherStage "*") -DestinationPath $launcherZip -CompressionLevel Optimal
    Remove-Item -LiteralPath $launcherStage -Recurse -Force

    $fileEntries = [ordered]@{}
    foreach ($assetName in @("MinecraftCopy.exe", "textureatlas.png")) {
        $assetPath = Join-Path $stagingDirectory $assetName
        $assetInfo = Get-Item -LiteralPath $assetPath
        $assetHash = (Get-FileHash -LiteralPath $assetPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $fileEntries[$assetName] = [ordered]@{
            sha256 = $assetHash
            size = [long]$assetInfo.Length
        }
    }

    $manifest = [ordered]@{
        schema = 1
        platform = "windows-x64"
        version = $Version
        files = $fileEntries
    }
    $manifestText = $manifest | ConvertTo-Json -Depth 5
    [System.IO.File]::WriteAllText(
        (Join-Path $stagingDirectory "game-manifest-windows-x64.json"),
        $manifestText + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )

    Move-Item -LiteralPath $stagingDirectory -Destination $releaseDirectory
    Write-Output "Release assets created in $releaseDirectory"
} finally {
    if (Test-Path -LiteralPath $stagingDirectory) {
        $resolvedOutputRoot = [System.IO.Path]::GetFullPath($outputRoot).TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar
        $resolvedStaging = [System.IO.Path]::GetFullPath($stagingDirectory)
        if (-not $resolvedStaging.StartsWith($resolvedOutputRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove staging outside the release output directory: $resolvedStaging"
        }
        Remove-Item -LiteralPath $resolvedStaging -Recurse -Force
    }
}
