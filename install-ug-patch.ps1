#Requires -Version 5.1
<#
.SYNOPSIS
    UG-Lab patch installer for OpenViBE.

.DESCRIPTION
    Installs the UG-Lab customised binaries and UI files into an existing
    OpenViBE installation.  A timestamped backup of every replaced file is
    created in a "backup" sub-folder next to this script.

    Patched components
    ------------------
    openvibe-acquisition-server.exe
        - EEGO-UG driver  (channel-name based selection, live SDK queries)
        - Multi-select in the 'Cap definition' channel-names dialog

    openvibe-plugins-simple-visualization.dll
        - Signal Display (UG): anti-aliased traces, dark/coloured background,
          live background and line colour pickers, stimulation flag markers

    openvibe-plugins-stimulation.dll
        - Keyboard Stimulator: GTK focus grab so keypresses are received
          immediately without clicking the panel first

    UI files
        - interface-EEGO-UG.ui          (EEGO-UG driver configuration dialog)
        - openvibe-simple-visualization-SignalDisplay.ui
                                        (adds Background / Line colour buttons)

.PARAMETER OpenViBEDir
    Root of the OpenViBE installation, e.g. C:\Program Files\openvibe-3.7.0
    If not supplied the script asks interactively.

.PARAMETER SourceDir
    Directory that contains the UG patch files (bin\ and share\ sub-folders).
    Defaults to the directory containing this script (i.e. the build output
    that was copied alongside it).
#>
param(
    [string] $OpenViBEDir = "",
    [string] $SourceDir   = $PSScriptRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Prompt-Directory([string] $message) {
    Add-Type -AssemblyName System.Windows.Forms | Out-Null
    $dlg = New-Object System.Windows.Forms.FolderBrowserDialog
    $dlg.Description         = $message
    $dlg.ShowNewFolderButton = $false
    if ($dlg.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
        return $dlg.SelectedPath
    }
    return $null
}

function Backup-File([string] $path, [string] $backupRoot) {
    if (Test-Path $path) {
        $rel  = $path.Substring($OpenViBEDir.Length).TrimStart('\','/')
        $dest = Join-Path $backupRoot $rel
        New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
        Copy-Item -Path $path -Destination $dest -Force
        Write-Host "  Backed up: $rel"
    }
}

function Install-File([string] $src, [string] $dest) {
    if (-not (Test-Path $src)) {
        Write-Warning "Source not found, skipping: $src"
        return
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null
    Copy-Item -Path $src -Destination $dest -Force
    $relDest = $dest.Substring($OpenViBEDir.Length).TrimStart('\','/')
    Write-Host "  Installed: $relDest"
}

# ---------------------------------------------------------------------------
# Resolve OpenViBE installation directory
# ---------------------------------------------------------------------------
if (-not $OpenViBEDir) {
    # Try common install locations first
    $candidates = @(
        "C:\Program Files\openvibe-3.7.0",
        "C:\Program Files (x86)\openvibe-3.7.0",
        "C:\openvibe-3.7.0"
    )
    foreach ($c in $candidates) {
        if (Test-Path (Join-Path $c "bin\openvibe-acquisition-server.exe")) {
            $OpenViBEDir = $c
            Write-Host "Auto-detected OpenViBE at: $OpenViBEDir"
            break
        }
    }
}

if (-not $OpenViBEDir -or -not (Test-Path $OpenViBEDir)) {
    Write-Host ""
    Write-Host "Please select the OpenViBE installation folder..."
    $OpenViBEDir = Prompt-Directory "Select your OpenViBE installation folder"
}

if (-not $OpenViBEDir -or -not (Test-Path $OpenViBEDir)) {
    Write-Error "OpenViBE installation folder not found or not selected. Aborting."
    exit 1
}

$binDir   = Join-Path $OpenViBEDir "bin"
$shareDir = Join-Path $OpenViBEDir "share\openvibe"

if (-not (Test-Path (Join-Path $binDir "openvibe-acquisition-server.exe"))) {
    Write-Error "'$OpenViBEDir' does not look like an OpenViBE installation (acquisition server not found). Aborting."
    exit 1
}

# ---------------------------------------------------------------------------
# Define patch manifest
# Each entry: source (relative to $SourceDir), destination (relative to $OpenViBEDir)
# ---------------------------------------------------------------------------
$manifest = @(
    # Binaries
    @{ src = "bin\openvibe-acquisition-server.exe";                                               dst = "bin\openvibe-acquisition-server.exe" },
    @{ src = "bin\openvibe-plugins-simple-visualization.dll";                                     dst = "bin\openvibe-plugins-simple-visualization.dll" },
    @{ src = "bin\openvibe-plugins-stimulation.dll";                                              dst = "bin\openvibe-plugins-stimulation.dll" },
    # UI files
    @{ src = "share\openvibe\applications\acquisition-server\interface-EEGO-UG.ui";              dst = "share\openvibe\applications\acquisition-server\interface-EEGO-UG.ui" },
    @{ src = "share\openvibe\plugins\simple-visualization\openvibe-simple-visualization-SignalDisplay.ui"; dst = "share\openvibe\plugins\simple-visualization\openvibe-simple-visualization-SignalDisplay.ui" }
)

# ---------------------------------------------------------------------------
# Show plan and confirm
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "=== UG-Lab OpenViBE patch ===" -ForegroundColor Cyan
Write-Host "Target : $OpenViBEDir"
Write-Host "Source : $SourceDir"
Write-Host ""
Write-Host "Files to install:"
foreach ($f in $manifest) {
    $src = Join-Path $SourceDir $f.src
    $exists = if (Test-Path $src) { "OK" } else { "MISSING" }
    Write-Host ("  [{0}] {1}" -f $exists, $f.dst)
}
Write-Host ""
$confirm = Read-Host "Proceed? (Y/N)"
if ($confirm -notmatch '^[Yy]') {
    Write-Host "Aborted." -ForegroundColor Yellow
    exit 0
}

# ---------------------------------------------------------------------------
# Backup existing files
# ---------------------------------------------------------------------------
$backupDir = Join-Path $PSScriptRoot ("backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
Write-Host ""
Write-Host "Backing up existing files to: $backupDir" -ForegroundColor Yellow

foreach ($f in $manifest) {
    $dest = Join-Path $OpenViBEDir $f.dst
    Backup-File $dest $backupDir
}

# ---------------------------------------------------------------------------
# Install patch files
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "Installing patch files..." -ForegroundColor Cyan

foreach ($f in $manifest) {
    $src  = Join-Path $SourceDir $f.src
    $dest = Join-Path $OpenViBEDir $f.dst
    Install-File $src $dest
}

Write-Host ""
Write-Host "=== Patch installed successfully ===" -ForegroundColor Green
Write-Host "Backup saved to: $backupDir"
Write-Host ""
Write-Host "Changes in this patch:"
Write-Host "  - EEGO-UG driver: channel selection via cap definition (named slots),"
Write-Host "    live SDK queries for channel count, sampling rates and ranges."
Write-Host "  - Channel names dialog: multi-select in left panel for block assignment."
Write-Host "  - Signal Display (UG): anti-aliased traces, colour pickers,"
Write-Host "    stimulation flag markers with stacking."
Write-Host "  - Keyboard Stimulator: immediate focus on dock embed."
