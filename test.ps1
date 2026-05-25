$ErrorActionPreference = "Stop"
$exePath = "D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64\Cyberpunk2077.exe"
$workDir = "D:\SteamLibrary\steamapps\common\Cyberpunk 2077\bin\x64"

Write-Host "Starting Cyberpunk 2077..."
Start-Process -FilePath $exePath -WorkingDirectory $workDir
Start-Sleep -Seconds 30
Write-Host "Closing Cyberpunk 2077..."
Stop-Process -Name Cyberpunk2077 -Force -ErrorAction SilentlyContinue
Write-Host "Test finished."
