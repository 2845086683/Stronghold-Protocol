[CmdletBinding()]
param(
    [string]$GameRoot = 'D:\Hypergryph Launcher\games\Arknights Game',
    [string]$PackageRoot,
    [string]$BackupRoot,
    [switch]$VerifyOnly,
    [switch]$Uninstall
)
$ErrorActionPreference = 'Stop'

function Get-CheckedLocalPath([string]$Value) {
    if ($Value -notmatch '^[A-Za-z]:[\\/]' -or $Value.Substring(2).Contains(':')) { throw 'An absolute local drive path without alternate streams is required.' }
    $taskPath = [IO.Path]::GetFullPath($Value).TrimEnd('\')
    $taskAncestor = $taskPath
    while ($taskAncestor) {
        if (Test-Path -LiteralPath $taskAncestor) {
            if (((Get-Item -LiteralPath $taskAncestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw "Reparse points are not supported: $taskAncestor" }
        }
        $taskParent = [IO.Path]::GetDirectoryName($taskAncestor)
        if ($taskParent -eq $taskAncestor) { break }
        $taskAncestor = $taskParent
    }
    return $taskPath
}
function Get-Sha256([string]$Path) {
    [void](Get-CheckedLocalPath $Path)
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Required file missing: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Assert-Digest([string]$Digest) { if ($Digest -cnotmatch '^[0-9a-f]{64}$') { throw 'Invalid SHA-256 in package or ownership record.' } }
function Write-NewFile([string]$Path, [byte[]]$Bytes) {
    $taskStream = [IO.File]::Open($Path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try { $taskStream.Write($Bytes, 0, $Bytes.Length); $taskStream.Flush($true) } finally { $taskStream.Dispose() }
}
function Assert-GameClosed {
    $taskProcesses = @(Get-CimInstance -ClassName Win32_Process -Filter "Name='Arknights.exe' OR Name='PlatformProcess.exe' OR Name='CefViewWing.exe'" -ErrorAction Stop)
    foreach ($taskProcess in $taskProcesses) {
        $taskExecutable = $taskProcess.ExecutablePath
        $taskPresence = $null
        try { $taskPresence = Get-Process -Id ([int]$taskProcess.ProcessId) -ErrorAction Stop }
        catch {
            if ($_.FullyQualifiedErrorId -like 'NoProcessFoundForGivenId,*') { continue }
            if (-not $taskExecutable) { throw 'Cannot determine a running game/helper process location; close it before changing this installation.' }
        }
        if ($taskPresence) {
            try {
                if ($taskPresence.HasExited) { continue }
                if (-not $taskExecutable) { try { $taskExecutable = $taskPresence.Path } catch { $taskExecutable = $null } }
            } finally { $taskPresence.Dispose() }
        }
        if (-not $taskExecutable) { throw 'Cannot determine a running game/helper process location; close it before changing this installation.' }
        if ($taskExecutable.StartsWith($taskRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Close the target game, PlatformProcess and CefViewWing before installation/removal.' }
    }
}
function Remove-IfOwned([string]$Path, [string]$Digest) {
    if ((Test-Path -LiteralPath $Path -PathType Leaf) -and (Get-Sha256 $Path) -eq $Digest) { Remove-Item -LiteralPath $Path }
}
function Restore-Original([string]$Digest) {
    if ((Get-Sha256 $taskOriginalBackup) -ne $Digest) { throw 'Original D3D compiler backup hash mismatch; refusing restoration.' }
    [IO.File]::Copy($taskOriginalBackup, $taskRestoreStage, $false)
    try {
        if ((Get-Sha256 $taskRestoreStage) -ne $Digest) { throw 'Staged restoration hash mismatch.' }
        [IO.File]::Replace($taskRestoreStage, $taskProxyTarget, [NullString]::Value)
        if ((Get-Sha256 $taskProxyTarget) -ne $Digest) { throw 'Restored original hash mismatch.' }
    } finally { Remove-IfOwned $taskRestoreStage $Digest }
}

$taskRoot = Get-CheckedLocalPath $GameRoot
if (-not (Test-Path -LiteralPath $taskRoot -PathType Container)) { throw 'GameRoot must be an existing directory.' }
$taskProxyTarget = Get-CheckedLocalPath (Join-Path $taskRoot 'd3dcompiler_47.dll')
$taskOriginalBackup = Get-CheckedLocalPath (Join-Path $taskRoot 'd3dcompiler_47.original.dll')
$taskProxyStage = Get-CheckedLocalPath (Join-Path $taskRoot 'd3dcompiler_47.StrongholdProtocolCN.staging.dll')
$taskRestoreStage = Get-CheckedLocalPath (Join-Path $taskRoot 'd3dcompiler_47.StrongholdProtocolCN.restore.dll')
$taskReceipt = Get-CheckedLocalPath (Join-Path $taskRoot 'd3dcompiler_47.StrongholdProtocolCN.install.json')
$taskPluginFolder = Get-CheckedLocalPath (Join-Path $taskRoot 'plugin\StrongholdProtocolCN')
$taskPluginTarget = Get-CheckedLocalPath (Join-Path $taskPluginFolder 'StrongholdCNActivity.dll')
$taskConfigTarget = Get-CheckedLocalPath (Join-Path $taskPluginFolder 'cn-activity.ini')
$taskOwnership = $null
if (Test-Path -LiteralPath $taskReceipt) {
    [void](Get-Sha256 $taskReceipt)
    $taskOwnership = Get-Content -LiteralPath $taskReceipt -Raw | ConvertFrom-Json
    foreach ($taskDigest in @($taskOwnership.originalSha256,$taskOwnership.proxySha256,$taskOwnership.pluginSha256,$taskOwnership.configSha256)) { Assert-Digest $taskDigest }
    if ($taskOwnership.product -ne 'StrongholdProtocol.CNActivity' -or
        $taskOwnership.gameRoot -ne $taskRoot -or $taskOwnership.proxyTarget -ne $taskProxyTarget -or
        $taskOwnership.originalBackup -ne $taskOriginalBackup -or $taskOwnership.pluginTarget -ne $taskPluginTarget -or
        $taskOwnership.configTarget -ne $taskConfigTarget -or $taskOwnership.proxyStage -ne $taskProxyStage -or
        $taskOwnership.restoreStage -ne $taskRestoreStage) { throw 'Ownership record does not own these exact target paths.' }
}

if ($Uninstall) {
    if (-not $taskOwnership) {
        if ((Test-Path -LiteralPath $taskOriginalBackup) -or (Test-Path -LiteralPath $taskPluginTarget) -or (Test-Path -LiteralPath $taskProxyStage)) { throw 'Possible unowned trial files found; refusing removal.' }
        Write-Output 'No owned D3D compiler trial installation found.'
        return
    }
    $taskCurrent = Get-Sha256 $taskProxyTarget
    if ($taskCurrent -ne $taskOwnership.proxySha256 -and $taskCurrent -ne $taskOwnership.originalSha256) { throw 'Current D3D compiler library differs from both owned original and proxy; refusing modification.' }
    if ((Test-Path -LiteralPath $taskOriginalBackup) -and (Get-Sha256 $taskOriginalBackup) -ne $taskOwnership.originalSha256) { throw 'Original backup has changed; refusing restoration/removal.' }
    if ($taskCurrent -eq $taskOwnership.proxySha256 -and -not (Test-Path -LiteralPath $taskOriginalBackup -PathType Leaf)) { throw 'The original backup needed to restore D3D compiler is missing.' }
    if ((Test-Path -LiteralPath $taskPluginTarget) -and (Get-Sha256 $taskPluginTarget) -ne $taskOwnership.pluginSha256) { throw 'Business plugin has changed; refusing removal.' }
    if ((Test-Path -LiteralPath $taskProxyStage) -and (Get-Sha256 $taskProxyStage) -ne $taskOwnership.proxySha256) { throw 'Staging file has changed; refusing removal.' }
    if (Test-Path -LiteralPath $taskRestoreStage) { throw 'Unexpected restoration stage exists; preserve it and resolve this collision first.' }
    if ($VerifyOnly) { Write-Output 'Owned D3D compiler trial removal and exact original restoration verified. No files written.'; return }
    Assert-GameClosed
    if ($taskCurrent -eq $taskOwnership.proxySha256) { Restore-Original $taskOwnership.originalSha256 }
    Remove-IfOwned $taskPluginTarget $taskOwnership.pluginSha256
    Remove-IfOwned $taskConfigTarget $taskOwnership.configSha256
    Remove-IfOwned $taskProxyStage $taskOwnership.proxySha256
    Remove-IfOwned $taskOriginalBackup $taskOwnership.originalSha256
    $taskRemaining = @($taskPluginTarget,$taskProxyStage,$taskOriginalBackup,$taskRestoreStage) | Where-Object { Test-Path -LiteralPath $_ }
    if (@($taskRemaining).Count -ne 0) { throw 'Original D3D compiler has been restored, but changed trial files remain. Ownership record retained for recovery.' }
    Remove-Item -LiteralPath $taskReceipt
    Write-Output 'Restored the exact original D3D shader compiler library and removed owned trial files. Backups and captures retained.'
    if (Test-Path -LiteralPath $taskConfigTarget) { Write-Output 'Modified listener configuration retained.' }
    return
}

if (-not $PackageRoot) {
    if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'package-manifest.json')) { $PackageRoot = $PSScriptRoot }
    else { $PackageRoot = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))) 'dist\StrongholdCNActivity-0.1.57' }
}
$taskPackage = Get-CheckedLocalPath $PackageRoot
$taskSourceProxy = Get-CheckedLocalPath (Join-Path $taskPackage 'proxy\d3dcompiler_47.dll')
$taskSourcePlugin = Get-CheckedLocalPath (Join-Path $taskPackage 'plugin\StrongholdCNActivity.dll')
$taskManifestPath = Get-CheckedLocalPath (Join-Path $taskPackage 'package-manifest.json')
[void](Get-Sha256 $taskManifestPath)
$taskManifest = Get-Content -LiteralPath $taskManifestPath -Raw | ConvertFrom-Json
if ($taskManifest.architecture -ne 'Windows-x64' -or $taskManifest.deployment -ne 'cn-native-entry-preview' -or
    $taskManifest.defaultListenerMode -ne 'probe' -or $taskManifest.testsRun -ne $true -or $taskManifest.compilerProxyTests -ne 'passed' -or
    $taskManifest.proxyLibrary -ne 'd3dcompiler_47.dll' -or $taskManifest.originalLibrary -ne 'd3dcompiler_47.original.dll' -or $taskManifest.pluginLibrary -ne 'plugin\StrongholdProtocolCN\StrongholdCNActivity.dll') { throw 'A tested Windows x64 CN activity entry-preview package in probe mode is required.' }
foreach ($taskDigest in @($taskManifest.proxySha256,$taskManifest.pluginSha256,$taskManifest.originalSha256)) { Assert-Digest $taskDigest }
if ((Get-Sha256 $taskSourceProxy) -ne $taskManifest.proxySha256 -or (Get-Sha256 $taskSourcePlugin) -ne $taskManifest.pluginSha256) { throw 'Package binary hash mismatch. Nothing installed.' }
$taskRequiredFiles = @('Arknights.exe','ArknightsBase.dll','UnityPlayer.dll','GameAssembly.dll','Arknights_Data\il2cpp_data\Metadata\global-metadata.dat')
$taskNames = @($taskManifest.expectedClientFiles | ForEach-Object { $_.relative })
if ($taskNames.Count -ne 5 -or @(Compare-Object $taskRequiredFiles $taskNames).Count -ne 0 -or @($taskNames | Select-Object -Unique).Count -ne 5) { throw 'All five CN binary/metadata hashes are required exactly once.' }
$taskInfo = Get-CheckedLocalPath (Join-Path $taskRoot 'Arknights_Data\app.info')
[void](Get-Sha256 $taskInfo)
if ((Get-Content -LiteralPath $taskInfo -Raw) -notmatch '(?m)^Arknights\s*$') { throw 'Target app.info is not the CN client.' }
foreach ($taskEntry in $taskManifest.expectedClientFiles) {
    Assert-Digest $taskEntry.sha256
    $taskPath = Get-CheckedLocalPath (Join-Path $taskRoot $taskEntry.relative)
    if (-not $taskPath.StartsWith($taskRoot + '\',[StringComparison]::OrdinalIgnoreCase) -or (Get-Sha256 $taskPath) -ne $taskEntry.sha256) { throw "CN client build changed: $($taskEntry.relative). Nothing installed." }
}
if ($taskOwnership) {
    if ($taskOwnership.proxySha256 -ne $taskManifest.proxySha256 -or $taskOwnership.pluginSha256 -ne $taskManifest.pluginSha256 -or $taskOwnership.originalSha256 -ne $taskManifest.originalSha256) { throw 'A different owned trial is present. Verify/uninstall it before installing this package.' }
    if ((Get-Sha256 $taskProxyTarget) -ne $taskManifest.proxySha256 -or (Get-Sha256 $taskOriginalBackup) -ne $taskManifest.originalSha256 -or
        (Get-Sha256 $taskPluginTarget) -ne $taskManifest.pluginSha256 -or -not (Test-Path -LiteralPath $taskConfigTarget -PathType Leaf)) { throw 'Owned trial installation is incomplete or changed. Use -Uninstall to restore the original first.' }
    Write-Output 'This exact D3D compiler trial is already installed and verified.'
    return
}
if ((Get-Sha256 $taskProxyTarget) -ne $taskManifest.originalSha256) { throw 'Bundled D3D shader compiler library hash differs from the package. Nothing installed.' }
foreach ($taskCollision in @($taskOriginalBackup,$taskPluginTarget,$taskConfigTarget,$taskProxyStage,$taskRestoreStage)) {
    if (Test-Path -LiteralPath $taskCollision) { throw "Unowned destination already exists; refusing overwrite: $taskCollision" }
}
if ($VerifyOnly) { Write-Output 'Preflight passed: exact CN build, original D3D compiler, proxy and separate plugin verified; mode=probe. No files written.'; return }
Assert-GameClosed
if (-not $BackupRoot) { $BackupRoot = Join-Path ([IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))) 'backups\CN-compiler47' }
$taskBackups = Get-CheckedLocalPath $BackupRoot
$taskBackupFolder = Get-CheckedLocalPath (Join-Path $taskBackups ([DateTimeOffset]::UtcNow.ToOffset([TimeSpan]::FromHours(8)).ToString('yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0,8)))
New-Item -ItemType Directory -Path $taskBackupFolder -Force | Out-Null
[IO.File]::Copy($taskProxyTarget,(Join-Path $taskBackupFolder 'd3dcompiler_47.dll'),$false)
if ((Get-Sha256 (Join-Path $taskBackupFolder 'd3dcompiler_47.dll')) -ne $taskManifest.originalSha256) { throw 'External original backup hash mismatch. No client modification performed.' }
Copy-Item -LiteralPath $taskManifestPath -Destination (Join-Path $taskBackupFolder 'package-manifest.json')
[ordered]@{ product='StrongholdProtocol.CNActivity.Backup'; gameRoot=$taskRoot; originalSha256=$taskManifest.originalSha256; originalTarget=$taskProxyTarget; createdUtc=[DateTimeOffset]::UtcNow.ToString('O'); expectedClientFiles=$taskManifest.expectedClientFiles } |
    ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $taskBackupFolder 'backup-manifest.json') -Encoding UTF8
$taskConfigBytes = [Text.UTF8Encoding]::new($false).GetBytes("[listener]`r`nmode=probe`r`n`r`n[backend]`r`nendpoint=http://127.0.0.1:16680`r`naccess_token=`r`n")
$taskHasher = [Security.Cryptography.SHA256]::Create()
try { $taskConfigHash = [BitConverter]::ToString($taskHasher.ComputeHash($taskConfigBytes)).Replace('-','').ToLowerInvariant() } finally { $taskHasher.Dispose() }
$taskReceiptData = [ordered]@{
    product='StrongholdProtocol.CNActivity'; version=$taskManifest.version; gameRoot=$taskRoot
    proxyTarget=$taskProxyTarget; originalBackup=$taskOriginalBackup; pluginTarget=$taskPluginTarget; configTarget=$taskConfigTarget
    proxyStage=$taskProxyStage; restoreStage=$taskRestoreStage; originalSha256=$taskManifest.originalSha256
    proxySha256=$taskManifest.proxySha256; pluginSha256=$taskManifest.pluginSha256; configSha256=$taskConfigHash
    defaultListenerMode='probe'; backupFolder=$taskBackupFolder; createdUtc=[DateTimeOffset]::UtcNow.ToString('O')
} | ConvertTo-Json -Depth 8
$taskReceiptBytes = [Text.UTF8Encoding]::new($false).GetBytes($taskReceiptData)
$taskCreatedReceipt=$false; $taskCreatedOriginal=$false; $taskCreatedPlugin=$false; $taskCreatedConfig=$false; $taskCreatedStage=$false
try {
    # An immutable ownership record precedes client mutations. An interrupted
    # prepared install is recoverable by -Uninstall even before replacement.
    Write-NewFile $taskReceipt $taskReceiptBytes
    $taskCreatedReceipt=$true
    [IO.File]::Copy($taskProxyTarget,$taskOriginalBackup,$false)
    $taskCreatedOriginal=$true
    if ((Get-Sha256 $taskOriginalBackup) -ne $taskManifest.originalSha256) { throw 'Same-directory original backup verification failed.' }
    New-Item -ItemType Directory -Path $taskPluginFolder -Force | Out-Null
    [IO.File]::Copy($taskSourcePlugin,$taskPluginTarget,$false)
    $taskCreatedPlugin=$true
    if ((Get-Sha256 $taskPluginTarget) -ne $taskManifest.pluginSha256) { throw 'Installed plugin verification failed.' }
    Write-NewFile $taskConfigTarget $taskConfigBytes
    $taskCreatedConfig=$true
    [IO.File]::Copy($taskSourceProxy,$taskProxyStage,$false)
    $taskCreatedStage=$true
    if ((Get-Sha256 $taskProxyStage) -ne $taskManifest.proxySha256 -or (Get-Sha256 $taskProxyTarget) -ne $taskManifest.originalSha256) { throw 'Original/staged proxy changed during installation.' }
    [IO.File]::Replace($taskProxyStage,$taskProxyTarget,[NullString]::Value)
    if ((Get-Sha256 $taskProxyTarget) -ne $taskManifest.proxySha256) { throw 'Installed proxy verification failed.' }
} catch {
    $taskFailure = $_
    $taskRestored=$false
    try {
        $taskCurrent = Get-Sha256 $taskProxyTarget
        if ($taskCurrent -eq $taskManifest.proxySha256 -and $taskCreatedOriginal) { Restore-Original $taskManifest.originalSha256; $taskRestored=$true }
        elseif ($taskCurrent -eq $taskManifest.originalSha256) { $taskRestored=$true }
        if ($taskRestored) {
            if ($taskCreatedPlugin) { Remove-IfOwned $taskPluginTarget $taskManifest.pluginSha256 }
            if ($taskCreatedConfig) { Remove-IfOwned $taskConfigTarget $taskConfigHash }
            if ($taskCreatedStage) { Remove-IfOwned $taskProxyStage $taskManifest.proxySha256 }
            if ($taskCreatedOriginal) { Remove-IfOwned $taskOriginalBackup $taskManifest.originalSha256 }
            $taskRemaining = @($taskPluginTarget,$taskConfigTarget,$taskProxyStage,$taskRestoreStage,$taskOriginalBackup) | Where-Object { Test-Path -LiteralPath $_ }
            if (@($taskRemaining).Count -eq 0 -and $taskCreatedReceipt) { Remove-Item -LiteralPath $taskReceipt }
            elseif (@($taskRemaining).Count -ne 0) { Write-Warning 'Rollback restored the original library, but changed or unexpected trial files remain. Ownership record retained.' }
        }
    } catch { Write-Warning 'Rollback could not be completed; immutable ownership record and verified backups retained for recovery.' }
    throw $taskFailure
}
Write-Output 'Installed the minimal D3D shader compiler forwarding shim plus separate plugin in probe mode. No game was launched and no IL2CPP hooks are enabled by default.'
Write-Output ("Verified original backup: {0}" -f $taskBackupFolder)
