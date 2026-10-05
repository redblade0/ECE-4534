[CmdletBinding(PositionalBinding=$false)]
param([string]$PythonPath = '',
      [Parameter(Position=0,ValueFromRemainingArguments=$true)][string[]]$AppArgs)
$ErrorActionPreference = 'Stop'
if (-not $PythonPath) { $PythonPath = Join-Path $PSScriptRoot '.venv\Scripts\python.exe' }
if (-not (Test-Path -LiteralPath $PythonPath)) { throw 'Create the .venv first, or pass -PythonPath.' }
& $PythonPath (Join-Path $PSScriptRoot 'app.py') @AppArgs
exit $LASTEXITCODE
