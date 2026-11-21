# Packs build\DisableWebs.dll into build\DisableWebs.script (zip: info.json + dll), the format Overstrike installs.
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build'
$stage = Join-Path $build 'pack'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null

$info = [ordered]@{
    name         = 'Disable Webs'
    author       = 'Quail0'
    version      = '1.0.0'
    game         = 'MSMR'
    type         = 'script'
    dependencies = @()
}
$info | ConvertTo-Json | Set-Content (Join-Path $stage 'info.json') -Encoding UTF8
Copy-Item (Join-Path $build 'DisableWebs.dll') $stage

$zip = Join-Path $build 'DisableWebs.zip'
$script = Join-Path $build 'DisableWebs.script'
Remove-Item $zip, $script -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Move-Item $zip $script
Write-Output "Packed $script"
