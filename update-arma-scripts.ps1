# Arma Reforger Scripts and Configs Extractor
# This script extracts .pak files from Arma Reforger and copies the needed files to the project

param(
    [switch]$SkipExtraction = $false,
    # Human-readable version label (e.g. "1.8.0") recorded in the tree's .version.json
    # and used to name the archive folder when this version is later superseded.
    [string]$VersionLabel = ""
)

# Paths configuration
$pakEntpackerExe = "N:\Temp\PakEntpacker\PakEntpacker.exe"
$armaReforgerAddons = "N:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons"
$targetPath = "N:\Projects\Arma 4\ArmaReforger"
$tempExtractPath = "N:\Temp\ArmaReforgerExtract"
$versionsPath = "N:\Projects\Arma 4\ArmaReforger.versions"
$appManifest = "N:\Program Files (x86)\Steam\steamapps\appmanifest_1874880.acf"
$markerName = ".version.json"

# Directories to copy (source pak dir -> folders to copy)
# Format: "addonDir/extractedFolder" = @("folder1", "folder2", ...)
# The extractedFolder name corresponds to the .pak file name (e.g., "data" -> "data.pak")
# Ordered: later paks layer over earlier ones, so merge order must be preserved
$extractionMap = [ordered]@{
    "data/data" = @("AI")
    "data/data007" = @("Configs", "Language","scripts","Prefabs","PrefabsEditable")
    "data/data010" = @("UI")
    "core/data" = @("scripts", "ui", "configs")
}

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Arma Reforger Scripts & Configs Update Tool" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host ""

# Verify PakEntpacker exists
if (-not (Test-Path $pakEntpackerExe)) {
    Write-Host "ERROR: PakEntpacker.exe not found at: $pakEntpackerExe" -ForegroundColor Red
    exit 1
}

# Verify Arma Reforger addons directory exists
if (-not (Test-Path $armaReforgerAddons)) {
    Write-Host "ERROR: Arma Reforger addons directory not found at: $armaReforgerAddons" -ForegroundColor Red
    exit 1
}

# Read the installed build id from the Steam app manifest (changes on every game update)
$newBuildId = $null
if (Test-Path $appManifest) {
    $manifestContent = Get-Content $appManifest -Raw
    if ($manifestContent -match '"buildid"\s+"(\d+)"') {
        $newBuildId = $Matches[1]
        Write-Host "Installed Arma Reforger build id: $newBuildId" -ForegroundColor Gray
    }
}
if (-not $newBuildId) {
    Write-Host "WARNING: Could not read buildid from $appManifest - version tracking will be incomplete" -ForegroundColor Yellow
}
Write-Host ""

# Extract .pak files
if (-not $SkipExtraction) {
    Write-Host "Step 1: Extracting .pak files..." -ForegroundColor Yellow
    Write-Host ""

    # Build list of unique pak files to extract from extractionMap
    $pakFilesToExtract = @{}
    foreach ($mapKey in $extractionMap.Keys) {
        $parts = $mapKey -split '/'
        $addonDir = $parts[0]
        $extractedFolder = $parts[1]
        $pakFileName = "$extractedFolder.pak"

        if (-not $pakFilesToExtract.ContainsKey($addonDir)) {
            $pakFilesToExtract[$addonDir] = @()
        }
        if ($pakFilesToExtract[$addonDir] -notcontains $pakFileName) {
            $pakFilesToExtract[$addonDir] += $pakFileName
        }
    }

    foreach ($addonDir in $pakFilesToExtract.Keys) {
        $pakDir = Join-Path $armaReforgerAddons $addonDir

        foreach ($pakFileName in $pakFilesToExtract[$addonDir]) {
            $pakFilePath = Join-Path $pakDir $pakFileName

            if (Test-Path $pakFilePath) {
                Write-Host "  Extracting $pakFileName..." -ForegroundColor Gray

                # Create temp directory for this pak file
                $tempPakDir = Join-Path $tempExtractPath ([System.IO.Path]::GetFileNameWithoutExtension($pakFileName))
                if (Test-Path $tempPakDir) {
                    Remove-Item -Path $tempPakDir -Recurse -Force
                }
                New-Item -Path $tempPakDir -ItemType Directory -Force | Out-Null

                # Copy pak file to temp
                $tempPakFile = Join-Path $tempPakDir $pakFileName
                Copy-Item -Path $pakFilePath -Destination $tempPakFile -Force

                # Extract in temp directory
                Push-Location $tempPakDir
                & $pakEntpackerExe
                $exitCode = $LASTEXITCODE
                Pop-Location

                if ($exitCode -ne 0) {
                    Write-Host "    WARNING: PakEntpacker returned error code $exitCode" -ForegroundColor Yellow
                }

                # Move extracted folder back to original location
                $extractedFolderName = [System.IO.Path]::GetFileNameWithoutExtension($pakFileName)
                $tempExtractedFolder = Join-Path $tempPakDir $extractedFolderName
                $finalExtractedFolder = Join-Path $pakDir $extractedFolderName

                if (Test-Path $tempExtractedFolder) {
                    # Remove old extracted folder if exists
                    if (Test-Path $finalExtractedFolder) {
                        Remove-Item -Path $finalExtractedFolder -Recurse -Force
                    }
                    # Move extracted content to final location
                    Move-Item -Path $tempExtractedFolder -Destination $finalExtractedFolder -Force
                    Write-Host "    Success!" -ForegroundColor Green
                } else {
                    Write-Host "    ERROR: Extraction failed - no output folder created" -ForegroundColor Red
                }

                # Clean up temp directory
                Remove-Item -Path $tempPakDir -Recurse -Force -ErrorAction SilentlyContinue
            } else {
                Write-Host "  WARNING: PAK file not found: $pakFilePath" -ForegroundColor Yellow
            }
        }
    }

    Write-Host ""
    Write-Host "  Extraction complete!" -ForegroundColor Green
    Write-Host ""
} else {
    Write-Host "Step 1: Skipping extraction (using existing extracted files)" -ForegroundColor Yellow
    Write-Host ""
}

# Archive or clean target directory
Write-Host "Step 2: Archiving/cleaning target directory..." -ForegroundColor Yellow
if (Test-Path $targetPath) {
    $oldMarkerPath = Join-Path $targetPath $markerName
    $oldMarker = $null
    if (Test-Path $oldMarkerPath) {
        try {
            $oldMarker = Get-Content $oldMarkerPath -Raw | ConvertFrom-Json
        } catch {
            Write-Host "  WARNING: Could not parse existing $markerName - treating tree as unlabeled" -ForegroundColor Yellow
        }
    }

    $sameBuild = $oldMarker -and $newBuildId -and ($oldMarker.buildId -eq $newBuildId)

    if ($sameBuild) {
        # Re-extraction of the same game build: replace in place, no archive
        Write-Host "  Same build ($newBuildId) already extracted - replacing in place (no archive)" -ForegroundColor Gray
        Remove-Item -Path $targetPath -Recurse -Force -ErrorAction Stop
        Write-Host "  Old files removed!" -ForegroundColor Green
    } else {
        # New game build (or unversioned tree): keep the old tree as a version archive
        if ($oldMarker -and $oldMarker.label) {
            $archiveName = $oldMarker.label
        } elseif ($oldMarker -and $oldMarker.buildId) {
            $archiveName = "build-$($oldMarker.buildId)"
        } else {
            $archiveName = "unlabeled-" + (Get-Item $targetPath).LastWriteTime.ToString('yyyy-MM-dd')
        }
        $archiveName = $archiveName -replace '[^\w\.\-]', '_'
        $archivePath = Join-Path $versionsPath $archiveName
        if (Test-Path $archivePath) {
            $archivePath = "$archivePath-" + (Get-Date -Format 'yyyyMMdd-HHmmss')
        }

        if (-not (Test-Path $versionsPath)) {
            New-Item -Path $versionsPath -ItemType Directory -Force | Out-Null
        }
        Write-Host "  Archiving previous version to: $archivePath" -ForegroundColor Gray
        Move-Item -Path $targetPath -Destination $archivePath -Force -ErrorAction Stop
        Write-Host "  Previous version archived!" -ForegroundColor Green
    }
} else {
    Write-Host "  Target directory doesn't exist yet" -ForegroundColor Gray
}
Write-Host ""

# Copy files to target
Write-Host "Step 3: Copying files to project..." -ForegroundColor Yellow
Write-Host ""

$totalCopied = 0
$totalFailed = 0
$foldersProcessed = @{}

foreach ($entry in $extractionMap.GetEnumerator()) {
    $sourcePath = Join-Path $armaReforgerAddons $entry.Key

    foreach ($folder in $entry.Value) {
        $sourceFolder = Join-Path $sourcePath $folder
        $targetFolder = Join-Path $targetPath $folder

        if (Test-Path $sourceFolder) {
            $isFirstCopy = -not $foldersProcessed.ContainsKey($folder)

            if ($isFirstCopy) {
                Write-Host "  Copying $folder..." -ForegroundColor Gray
                $foldersProcessed[$folder] = $true
            } else {
                Write-Host "  Merging additional $folder content..." -ForegroundColor Gray
            }

            Write-Host "    From: $sourceFolder" -ForegroundColor DarkGray
            Write-Host "    To:   $targetFolder" -ForegroundColor DarkGray

            try {
                if ($isFirstCopy) {
                    # First copy - copy the whole folder
                    Copy-Item -Path $sourceFolder -Destination $targetFolder -Recurse -Force -ErrorAction Stop
                    $fileCount = (Get-ChildItem -Path $targetFolder -Recurse -File).Count
                    Write-Host "    Success! ($fileCount files)" -ForegroundColor Green
                    $totalCopied++
                } else {
                    # Merge - copy contents into existing folder
                    $sourceFolderContents = Join-Path $sourceFolder "*"
                    Copy-Item -Path $sourceFolderContents -Destination $targetFolder -Recurse -Force -ErrorAction Stop
                    Write-Host "    Merged successfully!" -ForegroundColor Green
                }
            }
            catch {
                Write-Host "    ERROR: Failed to copy - $($_.Exception.Message)" -ForegroundColor Red
                if ($isFirstCopy) {
                    $totalFailed++
                }
            }
        }
        else {
            Write-Host "  WARNING: Source folder not found: $sourceFolder" -ForegroundColor Yellow
            $totalFailed++
        }
        Write-Host ""
    }
}

# Write version marker into the freshly extracted tree
Write-Host "Step 4: Writing version marker..." -ForegroundColor Yellow
if (Test-Path $targetPath) {
    if (-not $VersionLabel) {
        if ($newBuildId) {
            $VersionLabel = "build-$newBuildId"
        } else {
            $VersionLabel = "unlabeled-" + (Get-Date -Format 'yyyy-MM-dd')
        }
    }
    $marker = [ordered]@{
        buildId   = $newBuildId
        label     = $VersionLabel
        extracted = (Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
        source    = $armaReforgerAddons
    }
    $markerPath = Join-Path $targetPath $markerName
    $marker | ConvertTo-Json | Set-Content -Path $markerPath -Encoding UTF8
    Write-Host "  Wrote $markerPath (label: $VersionLabel, build: $newBuildId)" -ForegroundColor Green
} else {
    Write-Host "  WARNING: Target directory missing - nothing extracted?" -ForegroundColor Yellow
}
Write-Host ""

# Summary
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Summary" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Folders copied successfully: $totalCopied" -ForegroundColor Green
if ($totalFailed -gt 0) {
    Write-Host "Folders failed/not found: $totalFailed" -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Update complete! ArmaReforger reference tree is now: $VersionLabel (build $newBuildId)" -ForegroundColor Green
Write-Host ""
