# Windows deployment script
# Usage: .\deploy-windows.ps1 -TargetName "EZ4Connect" -DisplayName "EZ4Connect" -BuildDir "build" -Architecture "amd64"

param(
    [string]$TargetName = "EZ4Connect",
    [string]$DisplayName = "EZ4Connect",
    [string]$BuildDir = "build",
    [string]$Architecture = "amd64"
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
$ZjuUrl = "https://github.com/Mythologyli/zju-connect/releases/download/nightly/zju-connect-windows-$Architecture.zip"
$ZjuZipPath = "zju-connect-windows-$Architecture.zip"
Invoke-WebRequest -Uri $ZjuUrl -OutFile $ZjuZipPath
Expand-Archive -Path $ZjuZipPath -DestinationPath . -Force
Remove-Item -Path $ZjuZipPath

# Copy additional files
Copy-Item -Path "../libs/wintun/bin/$Architecture/wintun.dll" -Destination .
Copy-Item -Path "../resource/qt.conf" -Destination .

# Remove vc_redist executable
if ($Architecture -eq "amd64") {
    Remove-Item -Path vc_redist.x64.exe -ErrorAction SilentlyContinue
} else {
    Remove-Item -Path vc_redist.arm64.exe -ErrorAction SilentlyContinue
}

Pop-Location
