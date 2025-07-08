# Update Schemas to Current Branch
# Author: Mahima
# This script updates all config files to use schemas from the current git branch

# Get current branch name
$currentBranch = git rev-parse --abbrev-ref HEAD
Write-Host "Current branch: $currentBranch" -ForegroundColor Green

if (-not $currentBranch) {
    Write-Host "Error: Could not determine current branch" -ForegroundColor Red
    exit 1
}

Write-Host "Updating schema URLs to branch: $currentBranch" -ForegroundColor Cyan

# Function to update a single file
function Update-SchemaFile {
    param($FilePath)
    
    try {
        $content = Get-Content $FilePath -Raw
        $originalContent = $content
        
        # Update schema references to current branch
        $content = $content -replace 'https://raw\.githubusercontent\.com/imperialCHEPI/healthgps/[^/]+/schemas/', "https://raw.githubusercontent.com/imperialCHEPI/healthgps/$currentBranch/schemas/"
        
        if ($content -ne $originalContent) {
            Set-Content -Path $FilePath -Value $content
            Write-Host "  ✓ Updated: $FilePath" -ForegroundColor Yellow
            return $true
        }
        return $false
    }
    catch {
        Write-Host "  ✗ Error updating $FilePath`: $_" -ForegroundColor Red
        return $false
    }
}

# Update files in input-data directory
$updatedCount = 0
if (Test-Path "input-data") {
    Write-Host "Updating files in input-data..." -ForegroundColor Cyan
    
    $jsonFiles = Get-ChildItem -Path "input-data" -Recurse -Filter "*.json" -ErrorAction SilentlyContinue
    foreach ($file in $jsonFiles) {
        if (Update-SchemaFile $file.FullName) {
            $updatedCount++
        }
    }
}

# Update any config files in current directory
$configFiles = Get-ChildItem -Path "." -Filter "*.json" -ErrorAction SilentlyContinue
foreach ($file in $configFiles) {
    if (Update-SchemaFile $file.FullName) {
        $updatedCount++
    }
}

Write-Host "`nTotal files updated: $updatedCount" -ForegroundColor Green

if ($updatedCount -gt 0) {
    Write-Host "✅ Schema URLs now reference branch: $currentBranch" -ForegroundColor Green
} else {
    Write-Host "✅ No files needed updating (already using correct branch)" -ForegroundColor Green
}

Write-Host "`nYou can now run HealthGPS with schemas from the $currentBranch branch!" -ForegroundColor Cyan
