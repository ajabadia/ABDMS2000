<#
.SYNOPSIS
    Bundles the ABDMS2000 codebase into a single formatted Markdown file.

.DESCRIPTION
    Scans the ABDMS2000 project directory and concatenates all relevant source code files
    (C++, JavaScript, CSS, HTML, JSON Schemas, CMake, Build scripts) into a structured markdown document.
    Automatically excludes build artifacts, binary tables, node_modules, git directories, and large assets.

.PARAMETER OutputFile
    Path to the output markdown bundle. Default is "codebase_bundle.md" in the project root.

.PARAMETER IncludeDocs
    Switch to include markdown documentation from DOCS/ directory. Default is false.

.PARAMETER MaxFileSizeKB
    Maximum file size in KB to include. Default is 250 KB.

.EXAMPLE
    .\Scripts\bundle_code.ps1
    .\Scripts\bundle_code.ps1 -OutputFile "my_bundle.md" -IncludeDocs
#>

[CmdletBinding()]
param (
    [Parameter(Position = 0)]
    [string]$OutputFile = "codebase_bundle.md",

    [Parameter()]
    [switch]$IncludeDocs,

    [Parameter()]
    [int]$MaxFileSizeKB = 250
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$OutputPath = if ([System.IO.Path]::IsPathRooted($OutputFile)) { $OutputFile } else { Join-Path $ProjectRoot $OutputFile }

Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "         ABDMS2000 - Codebase Bundler" -ForegroundColor Cyan
Write-Host "=======================================================" -ForegroundColor Cyan
Write-Host "Project Root: $ProjectRoot"
Write-Host "Output File:  $OutputPath"
Write-Host ""

# Allowed source extensions
$AllowedExtensions = @(
    ".h", ".hpp", ".cpp", ".c", ".cc",
    ".js", ".mjs", ".ts",
    ".html", ".htm", ".css",
    ".json", ".cmake", ".txt", ".bat", ".ps1", ".yml", ".yaml", ".md"
)

# Directories to exclude
$ExcludeDirs = @(
    "build", "build_wasm", "bin", "obj", "out",
    "node_modules", ".git", ".github", ".vs", ".vscode",
    ".codebase-memory", ".system_generated", "CMakeFiles",
    "ABDMS2000_artefacts", "JuceLibraryCode"
)

# Files to exclude
$ExcludeFiles = @(
    "codebase_bundle.md",
    "package-lock.json",
    "yarn.lock",
    "pnpm-lock.yaml",
    "DW_8000_All.cpp",
    "BinaryData.cpp"
)

# Binary extensions to strictly avoid
$BinaryExtensions = @(
    ".exe", ".dll", ".lib", ".obj", ".vst3", ".component", ".a", ".so", ".dylib",
    ".zip", ".tar", ".gz", ".7z", ".rar",
    ".m1", ".wav", ".mp3", ".ogg", ".flac", ".syx", ".mid", ".midi",
    ".png", ".jpg", ".jpeg", ".gif", ".ico", ".svg", ".bmp", ".ttf", ".woff", ".woff2"
)

function Get-LanguageSyntax([string]$Extension) {
    switch ($Extension.ToLower()) {
        ".h"     { return "cpp" }
        ".hpp"   { return "cpp" }
        ".cpp"   { return "cpp" }
        ".c"     { return "c" }
        ".js"    { return "javascript" }
        ".mjs"   { return "javascript" }
        ".ts"    { return "typescript" }
        ".html"  { return "html" }
        ".css"   { return "css" }
        ".json"  { return "json" }
        ".cmake" { return "cmake" }
        ".bat"   { return "batchfile" }
        ".ps1"   { return "powershell" }
        ".yml"   { return "yaml" }
        ".yaml"  { return "yaml" }
        ".md"    { return "markdown" }
        default  { return "text" }
    }
}

Write-Host "Scanning codebase files..." -ForegroundColor Yellow

$CollectedFiles = [System.Collections.Generic.List[PSObject]]::new()
$AllFiles = Get-ChildItem -Path $ProjectRoot -Recurse -File

foreach ($File in $AllFiles) {
    $RelativePath = $File.FullName.Substring($ProjectRoot.Path.Length + 1).Replace("\", "/")
    $Extension = $File.Extension.ToLower()

    # Skip excluded directories
    $IsInExcludedDir = $false
    foreach ($Dir in $ExcludeDirs) {
        if ($RelativePath -match "(^|/)$Dir(/|$)") {
            $IsInExcludedDir = $true
            break
        }
    }
    if ($IsInExcludedDir) { continue }

    # Skip excluded files
    if ($ExcludeFiles -contains $File.Name) { continue }
    if ($File.Name.StartsWith("BinaryData") -and $Extension -eq ".cpp") { continue }
    if ($File.Name.EndsWith(".gen.cpp") -and $File.Length -gt 500000) { continue }

    # Skip DOCS directory if IncludeDocs is false
    if (!$IncludeDocs -and $RelativePath.StartsWith("DOCS/")) { continue }

    # Skip binaries
    if ($BinaryExtensions -contains $Extension) { continue }

    # Check allowed extension
    if ($AllowedExtensions -notcontains $Extension -and $File.Name -ne "CMakeLists.txt") {
        continue
    }

    # Check size limit
    $SizeKB = [Math]::Round($File.Length / 1024, 1)
    if ($File.Length -gt ($MaxFileSizeKB * 1024)) {
        Write-Warning "Skipping large file ($SizeKB KB): $RelativePath"
        continue
    }

    $CollectedFiles.Add([PSCustomObject]@{
        Path      = $RelativePath
        FullPath  = $File.FullName
        Extension = $Extension
        Size      = $File.Length
        SizeKB    = $SizeKB
    })
}

# Sort files logically
$SortedFiles = $CollectedFiles | Sort-Object {
    if ($_.Path -eq "CMakeLists.txt") { "00_CMakeLists.txt" }
    elseif ($_.Path.StartsWith("schemas/")) { "01_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/Core/")) { "02_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/DSP/")) { "03_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/MIDI/")) { "04_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/State/")) { "05_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/Plugin/")) { "06_" + $_.Path }
    elseif ($_.Path.StartsWith("Source/Wasm/")) { "07_" + $_.Path }
    elseif ($_.Path.StartsWith("WebUI/src/")) { "08_" + $_.Path }
    elseif ($_.Path.StartsWith("wasm/")) { "09_" + $_.Path }
    elseif ($_.Path.StartsWith("Scripts/")) { "10_" + $_.Path }
    else { "99_" + $_.Path }
}

Write-Host "Found $($SortedFiles.Count) files to bundle." -ForegroundColor Green
Write-Host "Writing bundle to $OutputPath..." -ForegroundColor Yellow

$Utf8NoBom = [System.Text.UTF8Encoding]::new($false)
$Writer = [System.IO.StreamWriter]::new($OutputPath, $false, $Utf8NoBom)

try {
    $Now = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $Writer.WriteLine("# ABDMS2000 - Codebase Bundle")
    $Writer.WriteLine("")
    $Writer.WriteLine("> Generated on: **$Now**  ")
    $Writer.WriteLine("> Total Files: **$($SortedFiles.Count)**  ")
    $Writer.WriteLine("")
    $Writer.WriteLine("---")
    $Writer.WriteLine("")
    $Writer.WriteLine("## Table of Contents")
    $Writer.WriteLine("")

    foreach ($Item in $SortedFiles) {
        $Anchor = $Item.Path.ToLower().Replace("/", "").Replace(".", "").Replace("-", "").Replace("_", "")
        $Writer.WriteLine("- [" + [char]96 + $Item.Path + [char]96 + "](#" + $Anchor + ") (" + $Item.SizeKB + " KB)")
    }

    $Writer.WriteLine("")
    $Writer.WriteLine("---")
    $Writer.WriteLine("")

    $ProcessedCount = 0
    $TotalLines = 0
    $Backtick = [string][char]96
    $TripleBacktick = $Backtick + $Backtick + $Backtick

    foreach ($Item in $SortedFiles) {
        $ProcessedCount++

        $Content = [System.IO.File]::ReadAllText($Item.FullPath, [System.Text.Encoding]::UTF8)
        $Lines = ($Content -split "\r?\n").Count
        $TotalLines += $Lines
        $Lang = Get-LanguageSyntax $Item.Extension

        $Writer.WriteLine("### " + $Backtick + $Item.Path + $Backtick)
        $Writer.WriteLine("")
        $Writer.WriteLine("*(Lines: " + $Lines + " | Size: " + $Item.SizeKB + " KB | Language: " + $Lang + ")*")
        $Writer.WriteLine("")
        $Writer.WriteLine($TripleBacktick + $Lang)
        $Writer.WriteLine($Content.TrimEnd())
        $Writer.WriteLine($TripleBacktick)
        $Writer.WriteLine("")
        $Writer.WriteLine("---")
        $Writer.WriteLine("")
    }
}

finally {
    $Writer.Dispose()
}

$OutputInfo = Get-Item $OutputPath
$SizeMB = [Math]::Round($OutputInfo.Length / 1048576, 2)
$OutKB = [Math]::Round($OutputInfo.Length / 1024, 1)

Write-Host ""
Write-Host "=======================================================" -ForegroundColor Green
Write-Host " Bundle Generated Successfully!" -ForegroundColor Green
Write-Host "=======================================================" -ForegroundColor Green
Write-Host (" Output File:  " + $OutputPath)
Write-Host (" Total Files:  " + $SortedFiles.Count)
Write-Host (" Total Lines:  " + $TotalLines)
Write-Host (" Bundle Size:  " + $SizeMB + " MB (" + $OutKB + " KB)")
Write-Host ""

