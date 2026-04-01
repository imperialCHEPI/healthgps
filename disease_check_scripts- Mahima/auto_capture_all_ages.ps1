# HealthGPS Automated Age Group Data Capture Script
# PowerShell version with better error handling

param(
    [string]$SimulationCommand = "your_simulation_command_here",
    [int]$TimeoutMinutes = 10
)

# Configuration
$SimulationCppPath = "src\HealthGPS\simulation.cpp"
$BuildTarget = "HealthGPS"
$OutputBaseDir = "age_group_data"
$InspectionDir = "risk_factor_inspection"

# Age groups to capture
$AgeGroups = @(
    @{MinAge = 0; MaxAge = 20; GroupName = "group_1_0_20"},
    @{MinAge = 21; MaxAge = 30; GroupName = "group_2_21_30"},
    @{MinAge = 31; MaxAge = 40; GroupName = "group_3_31_40"},
    @{MinAge = 41; MaxAge = 50; GroupName = "group_4_41_50"},
    @{MinAge = 51; MaxAge = 60; GroupName = "group_5_51_60"},
    @{MinAge = 61; MaxAge = 70; GroupName = "group_6_61_70"},
    @{MinAge = 71; MaxAge = 80; GroupName = "group_7_71_80"},
    @{MinAge = 81; MaxAge = 90; GroupName = "group_8_81_90"},
    @{MinAge = 91; MaxAge = 110; GroupName = "group_9_91_110"}
)

function Write-ColorOutput {
    param([string]$Message, [string]$Color = "White")
    Write-Host $Message -ForegroundColor $Color
}

function Create-Directories {
    Write-ColorOutput "📁 Creating output directories..." "Cyan"

    # Create base directory
    if (!(Test-Path $OutputBaseDir)) {
        New-Item -ItemType Directory -Path $OutputBaseDir | Out-Null
    }

    # Create subdirectories for each age group
    foreach ($group in $AgeGroups) {
        $groupDir = Join-Path $OutputBaseDir $group.GroupName
        if (!(Test-Path $groupDir)) {
            New-Item -ItemType Directory -Path $groupDir | Out-Null
        }
        Write-ColorOutput "   Created: $groupDir" "Green"
    }
}

function Update-SimulationCpp {
    param([int]$MinAge, [int]$MaxAge)

    Write-ColorOutput "🔧 Updating simulation.cpp for ages $MinAge-$MaxAge..." "Cyan"

    if (!(Test-Path $SimulationCppPath)) {
        Write-ColorOutput "❌ Error: $SimulationCppPath not found!" "Red"
        return $false
    }

    try {
        $content = Get-Content $SimulationCppPath -Raw
        $content = $content -replace 'int min_age = \d+;', "int min_age = $MinAge;"
        $content = $content -replace 'int max_age = \d+;', "int max_age = $MaxAge;"
        Set-Content -Path $SimulationCppPath -Value $content -NoNewline
        Write-ColorOutput "   Updated: min_age = $MinAge, max_age = $MaxAge" "Green"
        return $true
    }
    catch {
        Write-ColorOutput "❌ Error updating simulation.cpp: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Build-Project {
    Write-ColorOutput "🔨 Building HealthGPS project..." "Cyan"

    try {
        $buildArgs = @(
            "--build", "out\build\windows-release",
            "--target", $BuildTarget,
            "--config", "Release"
        )

        $process = Start-Process -FilePath "cmake" -ArgumentList $buildArgs -Wait -PassThru -NoNewWindow

        if ($process.ExitCode -eq 0) {
            Write-ColorOutput "   ✅ Build successful!" "Green"
            return $true
        } else {
            Write-ColorOutput "   ❌ Build failed with exit code $($process.ExitCode)" "Red"
            return $false
        }
    }
    catch {
        Write-ColorOutput "❌ Error building project: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Run-Simulation {
    Write-ColorOutput "🚀 Running HealthGPS simulation..." "Cyan"

    if ($SimulationCommand -eq "your_simulation_command_here") {
        Write-ColorOutput "⚠️  WARNING: Please update the simulation command in the script!" "Yellow"
        Write-ColorOutput "   Current command: $SimulationCommand" "Yellow"
        Write-ColorOutput "   Please edit this script and replace with your actual simulation command." "Yellow"
        return $false
    }

    try {
        $process = Start-Process -FilePath $SimulationCommand -Wait -PassThru -NoNewWindow

        if ($process.ExitCode -eq 0) {
            Write-ColorOutput "   ✅ Simulation completed successfully!" "Green"
            return $true
        } else {
            Write-ColorOutput "   ❌ Simulation failed with exit code $($process.ExitCode)" "Red"
            return $false
        }
    }
    catch {
        Write-ColorOutput "❌ Error running simulation: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Organize-OutputFiles {
    param([string]$GroupName)

    Write-ColorOutput "📦 Organizing output files for $GroupName..." "Cyan"

    $groupDir = Join-Path $OutputBaseDir $GroupName
    $inspectionPath = $InspectionDir

    if (!(Test-Path $inspectionPath)) {
        Write-ColorOutput "   ⚠️  Inspection directory $inspectionPath not found" "Yellow"
        return
    }

    $csvFiles = Get-ChildItem -Path $inspectionPath -Filter "*_inspection.csv"

    if ($csvFiles.Count -eq 0) {
        Write-ColorOutput "   ⚠️  No CSV files found in $inspectionPath" "Yellow"
        return
    }

    foreach ($file in $csvFiles) {
        $destPath = Join-Path $groupDir $file.Name
        Move-Item -Path $file.FullName -Destination $destPath
        Write-ColorOutput "   Moved: $($file.Name) -> $GroupName/" "Green"
    }

    # Clean up inspection directory
    Get-ChildItem -Path $inspectionPath -File | Remove-Item -Force
}

function Process-AgeGroup {
    param([hashtable]$Group)

    $minAge = $Group.MinAge
    $maxAge = $Group.MaxAge
    $groupName = $Group.GroupName

    Write-ColorOutput "`n$('='*60)" "Magenta"
    Write-ColorOutput "🎯 Processing Age Group: $minAge-$maxAge ($groupName)" "Magenta"
    Write-ColorOutput "$('='*60)" "Magenta"

    $startTime = Get-Date

    try {
        # Step 1: Update simulation.cpp
        if (!(Update-SimulationCpp -MinAge $minAge -MaxAge $maxAge)) {
            return $false
        }

        # Step 2: Build project
        if (!(Build-Project)) {
            return $false
        }

        # Step 3: Run simulation
        if (!(Run-Simulation)) {
            return $false
        }

        # Step 4: Organize output files
        Organize-OutputFiles -GroupName $groupName

        $elapsedTime = (Get-Date) - $startTime
        Write-ColorOutput "✅ Age group $minAge-$maxAge completed in $($elapsedTime.TotalSeconds.ToString('F1')) seconds" "Green"

        return $true
    }
    catch {
        Write-ColorOutput "❌ Error processing age group $minAge-$maxAge`: $($_.Exception.Message)" "Red"
        return $false
    }
}

function Create-SummaryReport {
    param([array]$SuccessfulGroups, [array]$FailedGroups)

    Write-ColorOutput "`n📊 Creating summary report..." "Cyan"

    $reportPath = Join-Path $OutputBaseDir "CAPTURE_SUMMARY.md"

    $reportContent = @"
# Age Group Data Capture Summary

Generated on: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')

## Captured Age Groups

| Group | Age Range | Directory | Status |
|-------|-----------|-----------|--------|
"@

    foreach ($group in $AgeGroups) {
        $groupDir = Join-Path $OutputBaseDir $group.GroupName
        $isSuccessful = $SuccessfulGroups -contains $group.GroupName

        if ($isSuccessful) {
            $csvFiles = Get-ChildItem -Path $groupDir -Filter "*.csv" -ErrorAction SilentlyContinue
            $fileCount = if ($csvFiles) { $csvFiles.Count } else { 0 }
            $reportContent += "`n| $($group.GroupName) | $($group.MinAge)-$($group.MaxAge) | $($group.GroupName)/ | ✅ $fileCount files |"
        } else {
            $reportContent += "`n| $($group.GroupName) | $($group.MinAge)-$($group.MaxAge) | $($group.GroupName)/ | ❌ Failed |"
        }
    }

    $reportContent += @"

## File Locations

- **Base Directory**: `$OutputBaseDir/`
- **Individual Groups**: `$OutputBaseDir/group_X_YY_ZZ/`
- **CSV Files**: `foodvegetable_inspection.csv`, `foodfruit_inspection.csv`

## Next Steps

1. Review the captured CSV files
2. Combine files if needed for analysis
3. Use the economist-friendly columns for plotting

## Summary

- **Successful groups**: $($SuccessfulGroups.Count)
- **Failed groups**: $($FailedGroups.Count)
- **Total groups**: $($AgeGroups.Count)
"@

    Set-Content -Path $reportPath -Value $reportContent
    Write-ColorOutput "   Summary report created: $reportPath" "Green"
}

# Main execution
Write-ColorOutput "🚀 HealthGPS Automated Age Group Data Capture" "Magenta"
Write-ColorOutput "=" * 50 "Magenta"

# Create directories
Create-Directories

# Process each age group
$successfulGroups = @()
$failedGroups = @()

foreach ($group in $AgeGroups) {
    $success = Process-AgeGroup -Group $group

    if ($success) {
        $successfulGroups += $group.GroupName
    } else {
        $failedGroups += $group.GroupName
    }

    # Small delay between runs
    Start-Sleep -Seconds 2
}

# Create summary report
Create-SummaryReport -SuccessfulGroups $successfulGroups -FailedGroups $failedGroups

# Final summary
Write-ColorOutput "`n$('='*60)" "Magenta"
Write-ColorOutput "📋 FINAL SUMMARY" "Magenta"
Write-ColorOutput "$('='*60)" "Magenta"
Write-ColorOutput "✅ Successful age groups: $($successfulGroups.Count)" "Green"
Write-ColorOutput "❌ Failed age groups: $($failedGroups.Count)" "Red"

if ($successfulGroups.Count -gt 0) {
    Write-ColorOutput "`nSuccessful groups:" "Green"
    foreach ($groupName in $successfulGroups) {
        Write-ColorOutput "  - $groupName" "Green"
    }
}

if ($failedGroups.Count -gt 0) {
    Write-ColorOutput "`nFailed groups:" "Red"
    foreach ($groupName in $failedGroups) {
        Write-ColorOutput "  - $groupName" "Red"
    }
}

Write-ColorOutput "`n📁 All data saved to: $OutputBaseDir/" "Cyan"
Write-ColorOutput "🎉 Automated capture process completed!" "Magenta"

Read-Host "`nPress Enter to continue..."
