#Requires -Version 5.1
<#
.SYNOPSIS
    Collect ALL UG-Lab patched files from the build output into patch\ and
    compile a cumulative Inno Setup installer.

.DESCRIPTION
    The patch is always CUMULATIVE: the installer always contains the full
    current set of every file we have modified.  Installing the latest
    patch-ug.exe on any lab machine brings it fully up to date, regardless
    of which earlier patches were applied.

    What "make-patch" does each time:
      1. Copy every file in the manifest from meta\build\ to patch\
      2. Record what changed since the previous run in patch\CHANGELOG.txt
      3. Bump the patch version in patch\VERSION
      4. Compile installer\patch-ug.exe with Inno Setup (if installed)

.PARAMETER BuildDir
    Root of the ninja build output.  Defaults to <repo>\meta\build.
#>
param(
    [string] $BuildDir = (Join-Path $PSScriptRoot "meta\build")
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$PatchDir   = Join-Path $PSScriptRoot "patch"
$Sentinel   = Join-Path $PatchDir ".last-built"
$Changelog  = Join-Path $PatchDir "CHANGELOG.txt"
$VersionFile= Join-Path $PatchDir "VERSION"
$IssFile    = Join-Path $PSScriptRoot "installer\patch-ug.iss"

# ---------------------------------------------------------------------------
# Complete manifest of every file we patch against the original OpenViBE 3.7
# Add new entries here whenever a new file is modified.
# ---------------------------------------------------------------------------
$manifest = @(
    @{ src = "bin\openvibe-acquisition-server.exe"
       dst = "bin\openvibe-acquisition-server.exe"
       note = "EEGO-UG driver; multi-select in cap definition dialog" },

    @{ src = "bin\openvibe-plugins-simple-visualization.dll"
       dst = "bin\openvibe-plugins-simple-visualization.dll"
       note = "Signal Display UG: anti-aliased traces, colour pickers, stimulation flags" },

    @{ src = "bin\openvibe-plugins-stimulation.dll"
       dst = "bin\openvibe-plugins-stimulation.dll"
       note = "Keyboard Stimulator: immediate focus on dock embed" },

    @{ src = "share\openvibe\applications\acquisition-server\interface-EEGO-UG.ui"
       dst = "share\openvibe\applications\acquisition-server\interface-EEGO-UG.ui"
       note = "EEGO-UG configuration dialog (new file)" },

    @{ src = "share\openvibe\plugins\simple-visualization\openvibe-simple-visualization-SignalDisplay.ui"
       dst = "share\openvibe\plugins\simple-visualization\openvibe-simple-visualization-SignalDisplay.ui"
       note = "Signal Display toolbar: Background and Line colour buttons" }
)

# ---------------------------------------------------------------------------
# Bump version  (YYYY.MMDD.N  where N resets each calendar day)
# ---------------------------------------------------------------------------
$today   = Get-Date -Format "yyyy.MMdd"
$prevVer = if (Test-Path $VersionFile) { Get-Content $VersionFile -Raw } else { "" }
$prevVer = $prevVer.Trim()

if ($prevVer -match "^$today\.(\d+)$") {
    $build = [int]$Matches[1] + 1
} else {
    $build = 1
}
$version = "$today.$build"
Write-Host ""
Write-Host "=== UG-Lab make-patch  v$version ===" -ForegroundColor Cyan
Write-Host "Build  : $BuildDir"
Write-Host "Patch  : $PatchDir"
Write-Host ""

# ---------------------------------------------------------------------------
# Determine what changed since last run (for changelog only)
# ---------------------------------------------------------------------------
$cutoff = if (Test-Path $Sentinel) { (Get-Item $Sentinel).LastWriteTime } `
          else { [datetime]::MinValue }

# ---------------------------------------------------------------------------
# Copy EVERY file unconditionally (cumulative patch)
# ---------------------------------------------------------------------------
$changes = @()

foreach ($f in $manifest) {
    $srcPath = Join-Path $BuildDir $f.src
    $dstPath = Join-Path $PatchDir $f.dst

    if (-not (Test-Path $srcPath)) {
        Write-Warning "  MISSING in build: $($f.src)"
        continue
    }

    $srcTime = (Get-Item $srcPath).LastWriteTime
    $null    = New-Item -ItemType Directory -Force (Split-Path $dstPath)
    Copy-Item $srcPath $dstPath -Force

    $isNew = $srcTime -gt $cutoff
    $tag   = if ($isNew) { "CHANGED" } else { "ok     " }
    $color = if ($isNew) { "Green"   } else { "DarkGray" }
    Write-Host ("  $tag  $($f.dst)") -ForegroundColor $color

    if ($isNew) { $changes += $f }
}

# ---------------------------------------------------------------------------
# Write changelog entry
# ---------------------------------------------------------------------------
$now = Get-Date
$entry = @"

=== v$version  $($now.ToString('yyyy-MM-dd HH:mm:ss')) ===
"@
if ($changes.Count -gt 0) {
    foreach ($f in $changes) { $entry += "`n  - $($f.dst)`n    $($f.note)" }
} else {
    $entry += "`n  (no files changed since last patch build)"
}

if (-not (Test-Path $Changelog)) {
    "UG-Lab patch changelog`n======================" | Set-Content $Changelog -Encoding utf8
}
$entry | Add-Content $Changelog -Encoding utf8
Write-Host ""
Write-Host "Changelog updated: $Changelog"

# ---------------------------------------------------------------------------
# Persist version and sentinel
# ---------------------------------------------------------------------------
$enc = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($VersionFile, $version, $enc)
[System.IO.File]::WriteAllText($Sentinel, $now.ToString("o"), $enc)

Write-Host "patch\ is ready. Commit and push to trigger the GitHub Action release." -ForegroundColor Green
Write-Host "  git add patch installer && git commit -m `"patch: v$version`" && git push"
Write-Host ""
