# TorGames Version Generator
# Generates a date-based version number: YYYY.MM.DD.BUILD
# If server is available, increments build number for same-day versions.

param(
    [string]$ServerUrl = "http://144.91.111.101:5001"
)

$today = Get-Date -Format "yyyy.MM.dd"
$buildNumber = 1

try {
    # Try to get existing versions from server
    $response = Invoke-RestMethod -Uri "$ServerUrl/api/update/versions" -Method Get -ErrorAction Stop -TimeoutSec 10

    if ($response -and $response.Count -gt 0) {
        # Find versions from today and get highest build number
        $todayVersions = $response | Where-Object { $_.version -like "$today.*" }

        if ($todayVersions) {
            $maxBuild = ($todayVersions | ForEach-Object {
                [int]($_.version -split '\.')[-1]
            } | Measure-Object -Maximum).Maximum
            $buildNumber = $maxBuild + 1
        }
    }
}
catch {
    # Server unavailable or no versions - use build 1
    Write-Host "Note: Could not reach server, using build number 1" -ForegroundColor Yellow
}

$version = "$today.$buildNumber"
Write-Output $version
