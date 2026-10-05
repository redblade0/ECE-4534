param(
    [string]$CcsRoot = 'C:\ti\ccs2101\ccs',
    [string]$SdkRoot = '',
    [string]$CompilerRoot = '',
    [string]$SysConfigRoot = ''
)
$ErrorActionPreference = 'Stop'
$taskRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (-not $SdkRoot) { $SdkRoot = Join-Path $PSScriptRoot 'SDK' }
if (-not $CompilerRoot) { $CompilerRoot = Join-Path $CcsRoot 'tools\compiler\ti-cgt-armllvm_5.1.1.LTS' }
if (-not $SysConfigRoot) { $SysConfigRoot = Join-Path $CcsRoot 'utils\sysconfig_1.28.1' }
$firmwareRoot = Join-Path $PSScriptRoot 'firmware'
$generatedRoot = Join-Path $firmwareRoot 'generated'
$buildRoot = Join-Path $firmwareRoot 'build'
New-Item -ItemType Directory -Force $generatedRoot,$buildRoot | Out-Null

& (Join-Path $SysConfigRoot 'sysconfig_cli.bat') `
    --product (Join-Path $SdkRoot '.metadata\product.json') `
    --compiler ticlang --output $generatedRoot (Join-Path $firmwareRoot 'joystick_starter.syscfg')
if ($LASTEXITCODE -ne 0) { throw 'SysConfig failed.' }

$compileArgs = @(
    '-mcpu=cortex-m0plus', '-march=thumbv6m', '-mthumb', '-mfloat-abi=soft',
    '-O2', '-g', '-Wall', '-Wextra', '-D__MSPM0G3507__', '-D__USE_SYSCONFIG__',
    ('-I' + $generatedRoot), ('-I' + $SdkRoot + '\source'),
    ('-I' + $SdkRoot + '\source\third_party\CMSIS\Core\Include'),
    (Join-Path $firmwareRoot 'main.c'),
    (Join-Path $generatedRoot 'ti_msp_dl_config.c'),
    (Join-Path $SdkRoot 'source\ti\devices\msp\m0p\startup_system_files\ticlang\startup_mspm0g350x_ticlang.c'),
    '-Wl,--rom_model', '-Wl,--warn_sections',
    ('-Wl,--map_file=' + $buildRoot + '\joystick_starter.map'),
    ('-L' + $SdkRoot + '\source'), ('-L' + $CompilerRoot + '\lib'), ('-L' + $generatedRoot),
    '-ldevice.cmd.genlibs', '-llibc.a', (Join-Path $generatedRoot 'device_linker.cmd'),
    '-o', (Join-Path $buildRoot 'joystick_starter.out')
)
& (Join-Path $CompilerRoot 'bin\tiarmclang.exe') @compileArgs
if ($LASTEXITCODE -ne 0) { throw 'Firmware build failed.' }
Write-Host ('Built ' + (Join-Path $buildRoot 'joystick_starter.out'))
