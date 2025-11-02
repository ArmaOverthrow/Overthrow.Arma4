# Arma Reforger Scripts and Configs Extractor
# This script extracts .pak files from Arma Reforger and copies the needed files to the project

param(
    [switch]$SkipExtraction = $false
)

# Paths configuration
$pakEntpackerExe = "N:\Temp\PakEntpacker\PakEntpacker.exe"
$armaReforgerAddons = "N:\SteamLibrary\steamapps\common\Arma Reforger\addons"
$targetPath = "N:\Projects\Arma 4\ArmaReforger"
$tempExtractPath = "N:\Temp\ArmaReforgerExtract"

# Directories to copy (source pak dir -> folders to copy)
# Format: "addonDir/extractedFolder" = @("folder1", "folder2", ...)
# The extractedFolder name corresponds to the .pak file name (e.g., "data" -> "data.pak")
$extractionMap = @{
    "data/data" = @("AI")
    "data/data007" = @("Configs", "Language","scripts","Prefabs","PrefabsEditable")
    "data/data009" = @("UI")
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

# Clean target directory
Write-Host "Step 2: Cleaning target directory..." -ForegroundColor Yellow
if (Test-Path $targetPath) {
    Write-Host "  Removing old files from: $targetPath" -ForegroundColor Gray
    Remove-Item -Path $targetPath -Recurse -Force -ErrorAction Stop
    Write-Host "  Old files removed!" -ForegroundColor Green
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

# Summary
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Summary" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "Folders copied successfully: $totalCopied" -ForegroundColor Green
if ($totalFailed -gt 0) {
    Write-Host "Folders failed/not found: $totalFailed" -ForegroundColor Yellow
}
Write-Host ""
Write-Host "Update complete! ArmaReforger scripts are now at version: $(Get-Date -Format 'yyyy-MM-dd')" -ForegroundColor Green
Write-Host ""
