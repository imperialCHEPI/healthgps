# Update Schema Files to Current Branch
# Author: Mahima
#
# This script automatically updates all schema files to reference the current git branch
# Usage: Run this script whenever you create a new branch or switch branches
# Language: Powershell scripting language

# Get current branch name from GitHub or when you create a new branch
$currentBranch = git rev-parse --abbrev-ref HEAD
Write-Host "Current branch: $currentBranch" -ForegroundColor Green

# Should not be happening- just a check
if (-not $currentBranch) {
    Write-Host "Error: Could not determine current branch" -ForegroundColor Red
    exit 1
}

# Function to update schema files in a directory
function Update-SchemaFiles {
    param(
        [string]$Path,
        [string]$BranchName
    )
    # keeps count of the number of files updated- in our case around 25
    $updatedCount = 0
    $schemaFiles = Get-ChildItem -Path $Path -Recurse -Filter "*.json"

    foreach ($file in $schemaFiles) {
        $content = Get-Content $file.FullName -Raw
        $originalContent = $content

        # Update $id fields (in schema definition files)
        $content = $content -replace 'https://raw\.githubusercontent\.com/imperialCHEPI/healthgps/[^/]+/schemas/', "https://raw.githubusercontent.com/imperialCHEPI/healthgps/$BranchName/schemas/"

        # Update $schema fields (in data files)
        $content = $content -replace '"\$schema":\s*"https://raw\.githubusercontent\.com/imperialCHEPI/healthgps/[^/]+/schemas/', "`"`$schema`": `"https://raw.githubusercontent.com/imperialCHEPI/healthgps/$BranchName/schemas/"

        # If file was changed, overwrite and log it
        if ($content -ne $originalContent) {
            Set-Content -Path $file.FullName -Value $content
            Write-Host "Updated: $($file.FullName)" -ForegroundColor Yellow
            $updatedCount++
        }
    }

    return $updatedCount
}

Write-Host "Updating schema files to use branch: $currentBranch" -ForegroundColor Cyan

# Update schema definition files
Write-Host "`nUpdating schema definition files..." -ForegroundColor Blue
$schemaCount = Update-SchemaFiles -Path "schemas" -BranchName $currentBranch

# Update data files that reference schemas
Write-Host "`nUpdating data files..." -ForegroundColor Blue
$dataCount = 0
if (Test-Path "input-data") {
    $dataCount = Update-SchemaFiles -Path "input-data" -BranchName $currentBranch
}

# Final count
$totalUpdated = $schemaCount + $dataCount
Write-Host "`nSummary:" -ForegroundColor Green
Write-Host "- Schema files updated: $schemaCount"
Write-Host "- Data files updated: $dataCount"
Write-Host "- Total files updated: $totalUpdated"

if ($totalUpdated -gt 0) {
    Write-Host "`n✅ All schema files now reference branch: $currentBranch" -ForegroundColor Green
    Write-Host "You can now commit these changes!" -ForegroundColor Green
} else {
    Write-Host "`n✅ All schema files were already up to date!" -ForegroundColor Green
}
