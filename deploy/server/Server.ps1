param(
    [Parameter(Position=0)][ValidateSet('Start','Stop','Status','Backup','Restore','Run','Logs')][string]$Action='Run',
    [string]$StateDirectory,
    [string]$PythonExe,
    [ValidateRange(1,65535)][int]$Port=16680,
    [ValidateSet('LAN','Local')][string]$Mode='LAN',
    [string]$BindAddress,
    [string]$Source,
    [string]$BackupDirectory
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$bundleRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
if (!$StateDirectory) { $StateDirectory=Join-Path $bundleRoot 'local-server\deployment' }
if (!$PythonExe) { $PythonExe=Join-Path $bundleRoot 'runtime\windows\python.exe' }
if (!(Test-Path -LiteralPath $PythonExe -PathType Leaf)) {
    throw 'No packaged Python runtime. Run Setup-Runtime.ps1 once, or pass -PythonExe with an installed CPython 3.12+ executable. No system Python is modified.'
}
$PythonExe=[IO.Path]::GetFullPath($PythonExe)
$version=& $PythonExe -I -c 'import sys,sqlite3,ssl; assert sys.version_info>=(3,12); print(sys.version.split()[0])'
if ($LASTEXITCODE -ne 0) { throw 'Python 3.12+ with sqlite3 and ssl is required.' }
$operation=$Action.ToLowerInvariant()
if ($Action -eq 'Run') { $operation='serve' }
$arguments=@('-I',(Join-Path $PSScriptRoot 'manage.py'),$operation,'--state-dir',$StateDirectory,'--port',"$Port")
if ($Action -in @('Start','Run')) {
    if (!$BindAddress) { $BindAddress=if($Mode -eq 'LAN'){'0.0.0.0'}else{'127.0.0.1'} }
    $arguments+=@('--host',$BindAddress)
    if ($Mode -eq 'LAN') { $arguments+='--public-only' }
}
if ($Source) { $arguments+=@('--source',$Source) }
if ($BackupDirectory) { $arguments+=@('--backup-dir',$BackupDirectory) }
& $PythonExe @arguments
if ($LASTEXITCODE -ne 0) { throw "Independent server $Action failed. See state directory logs." }
