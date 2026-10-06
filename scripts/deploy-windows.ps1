# Windows deployment script
# Usage: .\deploy-windows.ps1 -TargetName "EZ4Connect" -DisplayName "EZ4Connect" -BuildDir "build" -Architecture "amd64" -Nightly "false"

param(
    [string]$TargetName = "EZ4Connect",
    [string]$DisplayName = "EZ4Connect",
    [string]$BuildDir = "build",
    [string]$Architecture = "amd64",
    [string]$Nightly = "false"
)

Import-Module -Name Microsoft.PowerShell.Utility

# Create output directory
New-Item -ItemType Directory -Path "$DisplayName" -Force
Push-Location "$DisplayName"

# Copy executable
Copy-Item -Path "../$BuildDir/Release/$TargetName.exe" -Destination .

# Run windeployqt
& windeployqt.exe "$TargetName.exe"

# Download and extract zju-connect
$ZjuReleasePath = if ($Nightly -eq "true") { "download/nightly" } else { "latest/download" }
$ZjuUrl = "https://github.com/Mythologyli/zju-connect/releases/$ZjuReleasePath/zju-connect-windows-$Architecture.zip"
$ZjuZipPath = "zju-connect-windows-$Architecture.zip"
Invoke-WebRequest -Uri $ZjuUrl -OutFile $ZjuZipPath
Expand-Archive -Path $ZjuZipPath -DestinationPath . -Force
Remove-Item -Path $ZjuZipPath

# Copy additional files
Copy-Item -Path "../libs/wintun/bin/$Architecture/wintun.dll" -Destination .
Copy-Item -Path "../resource/qt.conf" -Destination .
Copy-Item -Path "../docs/THIRD_PARTY_NOTICES.txt" -Destination .

# Remove vc_redist executable
if ($Architecture -eq "amd64") {
    Remove-Item -Path vc_redist.x64.exe -ErrorAction SilentlyContinue
} else {
    Remove-Item -Path vc_redist.arm64.exe -ErrorAction SilentlyContinue
}

Pop-Location
