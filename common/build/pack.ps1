# Packs <ModDir>\build\<file>.dll into <ModDir>\build\<file>.script (zip: info.json + dll), the format Overstrike installs.
# The package metadata comes from <ModDir>\mod.json: { "file": "EndlessArena", "name": "EndlessArena", "version": "0.2.0" }
# (file = DLL/.script base name; name = display name in Overstrike). Author is the same for every mod (AGENTS.md rule 10).
param([Parameter(Mandatory)][string]$ModDir)
$ErrorActionPreference = 'Stop'
$mod = Get-Content -Raw (Join-Path $ModDir 'mod.json') | ConvertFrom-Json
$build = Join-Path $ModDir 'build'
$stage = Join-Path $build 'pack'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null

$info = [ordered]@{
    name         = $mod.name
    author       = 'Quail0'
    version      = $mod.version
    game         = 'MSMR'
    type         = 'script'
    dependencies = @()
}
$info | ConvertTo-Json | Set-Content (Join-Path $stage 'info.json') -Encoding UTF8
Copy-Item (Join-Path $build "$($mod.file).dll") $stage

$zip = Join-Path $build "$($mod.file).zip"
$script = Join-Path $build "$($mod.file).script"
Remove-Item $zip, $script -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip
Move-Item $zip $script
Write-Output "Packed $script"
