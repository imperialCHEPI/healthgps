param(
    [Parameter(Mandatory = $true)]
    [string]$ConsolePath,

    [Parameter(Mandatory = $true)]
    [string]$ConfigPath,

    [Parameter(Mandatory = $true)]
    [string]$LogPath,

    [int]$ThreadCount = 4,

    [switch]$DryRun
)

$ErrorActionPreference = "Continue"
$startMarker = "=== HealthGPS Studio run started ==="
$finishPrefix = "=== HealthGPS Studio run finished: exit"

Add-Content -Path $LogPath -Value $startMarker
Add-Content -Path $LogPath -Value "Command: $ConsolePath -c $ConfigPath (output.folder from config)"

$args = @(
    "-c", $ConfigPath,
    "-T", $ThreadCount,
    "--verbose"
)
if ($DryRun) {
    $args += "--dry-run"
}

& $ConsolePath @args 2>&1 | Tee-Object -FilePath $LogPath -Append
$exitCode = $LASTEXITCODE
Add-Content -Path $LogPath -Value "$finishPrefix $exitCode ==="
exit $exitCode
