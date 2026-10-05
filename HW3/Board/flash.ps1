param([string]$CcsRoot = 'C:\ti\ccs2101\ccs')
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$env:TI_APPDATA_DIR = Join-Path $PSScriptRoot '.workspace\ti-appdata'
New-Item -ItemType Directory -Force $env:TI_APPDATA_DIR | Out-Null
$config = Join-Path $PSScriptRoot 'firmware\targetConfigs\MSPM0G3507.ccxml'
$program = Join-Path $PSScriptRoot 'firmware\build\joystick_starter.out'
if (-not (Test-Path -LiteralPath $program)) { throw 'Run build.ps1 first.' }
& (Join-Path $CcsRoot 'ccs_base\DebugServer\bin\DSLite.exe') flash `
    --config=$config --core=0 --flash --verify --run --timeout=30 $program
if ($LASTEXITCODE -ne 0) { throw 'Flash/verification failed.' }
