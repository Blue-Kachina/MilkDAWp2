#Requires -Version 5.1
<#
.SYNOPSIS
  Idempotent native bootstrap for MilkDAWp 2 on Windows (Phase 0.12, D14).
.DESCRIPTION
  Installs (via winget) the minimum toolchain pinned in toolchain.json:
  Visual Studio 2022 Build Tools with the C++ workload, CMake, and Ninja.
  Safe to re-run.
.PARAMETER Doctor
  Report found vs. required versions and exit non-zero on any gap, without
  installing anything.
#>
param(
  [switch]$Doctor
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Toolchain = Get-Content (Join-Path $RepoRoot "toolchain.json") -Raw | ConvertFrom-Json

function Test-VersionAtLeast {
  param([string]$Found, [string]$Required)
  if ([string]::IsNullOrWhiteSpace($Found)) { return $false }
  $foundClean = ($Found -replace '[^\d\.].*$', '')
  if ([string]::IsNullOrWhiteSpace($foundClean)) { return $false }
  try {
    return [version]$foundClean -ge [version]$Required
  } catch {
    return $false
  }
}

$results = @()

# CMake
$cmakeFound = $null
if (Get-Command cmake -ErrorAction SilentlyContinue) {
  $line = (cmake --version | Select-Object -First 1)
  if ($line -match '(\d+\.\d+\.\d+)') { $cmakeFound = $Matches[1] }
}
$results += [pscustomobject]@{
  Tool = "cmake"; Required = $Toolchain.cmake.minVersion; Found = $cmakeFound
  Ok = (Test-VersionAtLeast $cmakeFound $Toolchain.cmake.minVersion)
}

# Ninja
$ninjaFound = $null
if (Get-Command ninja -ErrorAction SilentlyContinue) {
  $line = (ninja --version | Select-Object -First 1)
  if ($line -match '(\d+\.\d+(\.\d+)?)') { $ninjaFound = $Matches[1] }
}
$results += [pscustomobject]@{
  Tool = "ninja"; Required = $Toolchain.ninja.minVersion; Found = $ninjaFound
  Ok = (Test-VersionAtLeast $ninjaFound $Toolchain.ninja.minVersion)
}

# Visual Studio with the C++ (VCTools) workload, via vswhere
$vsFound = $null
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  $vsFound = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationVersion
}
$results += [pscustomobject]@{
  Tool = "Visual Studio (C++ workload)"; Required = $Toolchain.windows.visualStudio.minVersion; Found = $vsFound
  Ok = (Test-VersionAtLeast $vsFound $Toolchain.windows.visualStudio.minVersion)
}

$results | Format-Table -Property Tool, Required, Found, Ok -AutoSize

$missing = $results | Where-Object { -not $_.Ok }

if ($Doctor) {
  if ($missing) { exit 1 } else { exit 0 }
}

if (-not $missing) {
  Write-Host "All required tools already meet the minimum version. Nothing to install."
  exit 0
}

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
  Write-Error "winget not found. Install 'App Installer' from the Microsoft Store, or install the missing tools manually (see toolchain.json)."
  exit 1
}

foreach ($m in $missing) {
  switch ($m.Tool) {
    "cmake" {
      winget install --id Kitware.CMake -e --accept-source-agreements --accept-package-agreements
    }
    "ninja" {
      winget install --id Ninja-build.Ninja -e --accept-source-agreements --accept-package-agreements
    }
    "Visual Studio (C++ workload)" {
      winget install --id Microsoft.VisualStudio.2022.BuildTools -e `
        --accept-source-agreements --accept-package-agreements `
        --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --quiet --norestart"
    }
  }
}

Write-Host "Bootstrap complete. Re-run with -Doctor to verify."
