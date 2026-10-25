[CmdletBinding()]
param(
    [ValidateSet('Install','Configure','Uninstall','Verify')][string]$Action = 'Install',
    [string]$GameRoot,
    [string]$Endpoint,
    [string]$PackageRoot,
    [string]$StateRoot,
    [switch]$NonInteractive
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2.0
# Pin the host's own utility module even when launched from a PowerShell 7
# terminal whose PSModulePath also contains incompatible newer modules.
Import-Module (Join-Path $PSHOME 'Modules\Microsoft.PowerShell.Utility\Microsoft.PowerShell.Utility.psd1') -ErrorAction Stop

# WinPS 5.1 file providers retain MAX_PATH limits. Use Unicode extended paths
# only inside this boundary; no machine-wide long-path setting is changed.
if (-not ('StrongholdDeployment.NativeFile' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Win32.SafeHandles;
namespace StrongholdDeployment {
 public static class NativeFile {
  const uint Invalid=0xffffffff, Directory=0x10, Reparse=0x400;
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern uint GetFullPathNameW(string p,uint n,StringBuilder b,IntPtr part);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern uint GetFileAttributesW(string p);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateDirectoryW(string p,IntPtr security);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CopyFileW(string s,string d,bool failIfExists);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool DeleteFileW(string p);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool MoveFileExW(string s,string d,uint flags);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ReplaceFileW(string d,string s,string backup,uint flags,IntPtr exclude,IntPtr reserved);
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern SafeFileHandle CreateFileW(string p,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
  static void Fail(string operation,string path) { throw new Win32Exception(Marshal.GetLastWin32Error(),operation+": "+path); }
  public static string Full(string value) {
   if (String.IsNullOrEmpty(value) || value.Length<3 || !((value[0]>='A' && value[0]<='Z') || (value[0]>='a' && value[0]<='z')) || value[1]!=':' || (value[2]!='\\' && value[2]!='/') || value.Substring(2).Contains(":")) throw new IOException("An absolute local drive path without alternate streams is required.");
   string input=value.Replace('/','\\');
   foreach(string part in input.Substring(3).Split('\\')) {
    if (part=="." || part==".." || part.Length==0) continue;
    if (part.Length>255 || part.EndsWith(".") || part.EndsWith(" ") || part.IndexOfAny(new char[]{'<','>','\"','|','?','*'})>=0) throw new IOException("Invalid local path component.");
    foreach(char c in part) if(c<32) throw new IOException("Invalid local path component.");
   }
   var result=new StringBuilder(32768); uint count=GetFullPathNameW(input,(uint)result.Capacity,result,IntPtr.Zero);
   if(count==0) Fail("Resolve local path",value);
   if(count>=32760) throw new IOException("Local path exceeds the supported Unicode path length.");
   string full=result.ToString(); return full.Length>3?full.TrimEnd('\\'):full;
  }
  public static string Parent(string path) { string p=Full(path); if(p.Length<=3) return null; int n=p.LastIndexOf('\\'); return n==2?p.Substring(0,3):p.Substring(0,n); }
  static string Extended(string full) { return @"\\?\"+full; }
  static uint Attributes(string full) {
   uint attrs=GetFileAttributesW(Extended(full));
   if(attrs==Invalid) { int error=Marshal.GetLastWin32Error(); if(error==2 || error==3) return Invalid; Fail("Read file attributes",full); }
   return attrs;
  }
  public static string Checked(string value) {
   string full=Full(value), ancestor=full;
   while(ancestor!=null) { uint a=Attributes(ancestor); if(a!=Invalid && (a&Reparse)!=0) throw new IOException("Reparse points are not supported: "+ancestor); ancestor=Parent(ancestor); }
   return full;
  }
  public static bool Exists(string value,string kind) {
   uint a=Attributes(Checked(value)); if(a==Invalid) return false;
   return kind=="Leaf"?(a&Directory)==0:kind=="Container"?(a&Directory)!=0:true;
  }
  public static void MakeDirectory(string value) {
   string p=Checked(value); uint a=Attributes(p);
   if(a!=Invalid) { if((a&Directory)==0) throw new IOException("A file occupies the required directory: "+p); return; }
   string parent=Parent(p); if(parent!=null) MakeDirectory(parent);
   if(!CreateDirectoryW(Extended(p),IntPtr.Zero)) { int e=Marshal.GetLastWin32Error(); if(e!=183 || !Exists(p,"Container")) Fail("Create directory",p); }
  }
  static FileStream Open(string value,bool write) {
   string p=Checked(value); SafeFileHandle h=CreateFileW(Extended(p),write?0x40000000u:0x80000000u,write?0u:1u,IntPtr.Zero,write?1u:3u,0x80,IntPtr.Zero);
   if(h.IsInvalid) { h.Dispose(); Fail(write?"Create file":"Open file",p); }
   try { return new FileStream(h,write?FileAccess.Write:FileAccess.Read,65536,false); } catch { h.Dispose(); throw; }
  }
  public static string Hash(string path) { using(var f=Open(path,false)) using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant(); }
  public static byte[] ReadBytes(string path) { using(var f=Open(path,false)) using(var m=new MemoryStream()) { f.CopyTo(m); return m.ToArray(); } }
  public static string ReadText(string path) { using(var f=Open(path,false)) using(var r=new StreamReader(f,Encoding.UTF8,true)) return r.ReadToEnd(); }
  public static void WriteNew(string path,byte[] bytes) { using(var f=Open(path,true)) { f.Write(bytes,0,bytes.Length); f.Flush(true); } }
  public static void Copy(string source,string target) { string s=Checked(source),d=Checked(target); if(!CopyFileW(Extended(s),Extended(d),true)) Fail("Copy file",d); }
  public static void Commit(string stage,string target) {
   string s=Checked(stage),d=Checked(target);
   if(Exists(d,"Leaf")) { if(!ReplaceFileW(Extended(d),Extended(s),null,0,IntPtr.Zero,IntPtr.Zero)) Fail("Replace file",d); }
   else if(!MoveFileExW(Extended(s),Extended(d),8)) Fail("Commit file",d);
  }
  public static void Delete(string path) { string p=Checked(path); if(!DeleteFileW(Extended(p))) Fail("Delete file",p); }
 }
}
'@
}
function Get-LocalPath([string]$Value) {
    $taskPath=[StrongholdDeployment.NativeFile]::Checked($Value)
    if ($taskPath.Length -le 3) { throw 'A drive root cannot be used as a game, package or state directory.' }
    return $taskPath
}
function Test-LocalPath([string]$Path,[string]$Kind='Any') { return [StrongholdDeployment.NativeFile]::Exists($Path,$Kind) }
function Read-LocalText([string]$Path) { return [StrongholdDeployment.NativeFile]::ReadText($Path) }
function Get-Digest([string]$Path) {
    [void](Get-LocalPath $Path)
    if (-not (Test-LocalPath $Path 'Leaf')) { throw "Required file missing: $Path" }
    return [StrongholdDeployment.NativeFile]::Hash($Path)
}
function Assert-Hash([string]$Path,[string]$Digest) {
    if ($Digest -cnotmatch '^[0-9a-f]{64}$' -or (Get-Digest $Path) -cne $Digest) { throw "Hash mismatch: $Path" }
}
function Get-ChildPath([string]$Parent,[string]$Relative) {
    if (-not $Relative -or [IO.Path]::IsPathRooted($Relative) -or $Relative.Contains(':')) { throw 'A relative package path is required.' }
    $taskChild = Get-LocalPath ($Parent.TrimEnd('\')+'\'+$Relative)
    if (-not $taskChild.StartsWith($Parent+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Package path escapes its root.' }
    return $taskChild
}
function Write-Atomic([string]$Path,[byte[]]$Bytes) {
    [void](Get-LocalPath $Path)
    $taskStage=([StrongholdDeployment.NativeFile]::Parent($Path))+'\.spcn-'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try {
        [StrongholdDeployment.NativeFile]::WriteNew($taskStage,$Bytes)
        [StrongholdDeployment.NativeFile]::Commit($taskStage,$Path)
    } finally { if (Test-LocalPath $taskStage 'Leaf') { [StrongholdDeployment.NativeFile]::Delete($taskStage) } }
}
function Write-Json([string]$Path,$Value) { Write-Atomic $Path ([Text.UTF8Encoding]::new($false).GetBytes(($Value | ConvertTo-Json -Depth 30))) }
function Read-Property($Value,[string]$Name) { if ($Value -and $Value.PSObject.Properties[$Name]) { return $Value.$Name }; return $null }
function Invoke-InstallerEngine {
    param([string]$GameRoot,[string]$PackageRoot,[string]$BackupRoot,[switch]$VerifyOnly,[switch]$Uninstall)
    # The unchanged engine writes UTF8 JSON. Pin its unqualified Get-Content reads
    # to UTF8 in this invocation, including non-ASCII game/backup directories.
    $PSDefaultParameterValues=@{'Get-Content:Encoding'='UTF8'}
    $taskArguments=@{GameRoot=$GameRoot}
    if ($PackageRoot) { $taskArguments.PackageRoot=$PackageRoot }
    if ($BackupRoot) { $taskArguments.BackupRoot=$BackupRoot }
    if ($VerifyOnly) { $taskArguments.VerifyOnly=$true }
    if ($Uninstall) { $taskArguments.Uninstall=$true }
    & $taskEngine @taskArguments
}
function Assert-InstallerPathLengths([string]$Root,[string]$Package,[string]$State) {
    # Overlay paths use NativeFile; the unchanged, separately hashed DLL engine
    # still uses framework/provider I/O. Reject its unsupported roots before any
    # backup or mutation instead of failing halfway through an installation.
    $taskPaths=@((Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.restore.dll'),(Join-Path $Root 'plugin\StrongholdProtocolCN\StrongholdCNActivity.dll'),(Join-Path $Package 'plugin\StrongholdCNActivity.dll'),(Join-Path $Package 'proxy\d3dcompiler_47.dll'),(Join-Path $State 'backups\00000000-000000-00000000000000000000000000000000\rollback-original\00000000-000000-00000000\package-manifest.json'))
    foreach ($taskRow in $taskManifest.expectedClientFiles) { $taskPaths += Get-ChildPath $Root $taskRow.relative }
    foreach ($taskPath in $taskPaths) { if ($taskPath.Length -ge 260) { throw 'The unchanged DLL installer requires a shorter game, extraction or backup root. Move the extraction closer to the drive root or choose a shorter persistent -StateRoot. Long overlay resource paths themselves are supported.' } }
}
function Get-Origin([string]$Value) {
    # Match the native endpoint parser: ASCII domain/IPv4 + optional TCP port.
    if ($Value -cnotmatch '^https?://[A-Za-z0-9](?:[A-Za-z0-9.-]*[A-Za-z0-9])?(?::[0-9]{1,5})?/?$') { throw 'Endpoint must be an HTTP/HTTPS origin, for example https://game.example.com (no path, credentials, query or IPv6).' }
    $taskUri = [Uri]$Value
    $taskAuthority=([regex]::Match($Value.TrimEnd('/'),'^https?://(.+)$')).Groups[1].Value
    if ($taskUri.Port -lt 1 -or $taskUri.Port -gt 65535 -or $taskAuthority.Length -gt 253 -or $taskUri.Host.StartsWith('-') -or $taskUri.Host.EndsWith('-')) { throw 'Endpoint host or port is invalid.' }
    return $Value.TrimEnd('/')
}
function Get-ServerPublicMode([string]$Origin) {
    try { $taskHealth=Invoke-RestMethod -Uri ($Origin+'/health') -TimeoutSec 2 -ErrorAction Stop } catch { return $null }
    if (-not $taskHealth.ok -or $taskHealth.purpose -ne 'CN native activity private backend' -or $taskHealth.officialWireCompatible -ne $false) { throw 'The selected origin replied with an unexpected server identity.' }
    $taskPublic=Read-Property $taskHealth 'publicMultiplayerOnly'
    if ($null -ne $taskPublic -and $taskPublic -isnot [bool]) { throw 'Invalid publicMultiplayerOnly in server health.' }
    return $taskPublic
}
function Assert-Closed([string]$Root) {
    foreach ($taskProcess in @(Get-CimInstance Win32_Process -Filter "Name='Arknights.exe' OR Name='PlatformProcess.exe' OR Name='CefViewWing.exe'" -ErrorAction Stop)) {
        # CIM can retain a terminated helper after its OS exit code is final.
        # Confirm liveness before treating that stale record as a running game.
        $taskProcessErrors=@()
        $taskNative=Get-Process -Id $taskProcess.ProcessId -ErrorAction SilentlyContinue -ErrorVariable taskProcessErrors
        foreach($taskProcessError in $taskProcessErrors) {
            if($taskProcessError.FullyQualifiedErrorId -notlike 'NoProcessFoundForGivenId,*') { throw $taskProcessError }
        }
        if($null -eq $taskNative) { continue }
        try { if($taskNative.HasExited) { continue } }
        finally { if($taskNative -is [System.IDisposable]) { $taskNative.Dispose() } }
        if (-not $taskProcess.ExecutablePath -or $taskProcess.ExecutablePath.StartsWith($Root+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Close the target game and its PlatformProcess/CefViewWing helpers before deployment.' }
    }
}
function Assert-Package($Manifest,[string]$Root) {
    if ($Manifest.architecture -ne 'Windows-x64' -or $Manifest.deployment -ne 'cn-native-entry-preview' -or $Manifest.testsRun -ne $true -or $Manifest.compilerProxyTests -ne 'passed' -or $Manifest.defaultListenerMode -ne 'probe' -or $Manifest.privateBackendAutostart -ne $false -or $Manifest.privateBackendStartupMode -ne 'independent') { throw 'A tested Windows-x64 independent-backend package is required.' }
    if ($Manifest.proxyLibrary -ne 'd3dcompiler_47.dll' -or $Manifest.originalLibrary -ne 'd3dcompiler_47.original.dll' -or $Manifest.pluginLibrary -ne 'plugin\StrongholdProtocolCN\StrongholdCNActivity.dll') { throw 'Unsupported package target layout.' }
    if (-not $Manifest.version -or $Manifest.originalSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Package version/original hash is missing.' }
    Assert-Hash (Join-Path $Root 'proxy\d3dcompiler_47.dll') $Manifest.proxySha256
    Assert-Hash (Join-Path $Root 'plugin\StrongholdCNActivity.dll') $Manifest.pluginSha256
    $taskNames = @($Manifest.expectedClientFiles | ForEach-Object { $_.relative })
    $taskExpected = @('Arknights.exe','ArknightsBase.dll','UnityPlayer.dll','GameAssembly.dll','Arknights_Data\il2cpp_data\Metadata\global-metadata.dat')
    if ($taskNames.Count -ne 5 -or @(Compare-Object $taskNames $taskExpected).Count -ne 0 -or @($taskNames | Select-Object -Unique).Count -ne 5) { throw 'All five exact CN build hashes are required.' }
}
function Assert-Client($Manifest,[string]$Root) {
    $taskInfo = Get-ChildPath $Root 'Arknights_Data\app.info'
    [void](Get-Digest $taskInfo)
    if ((Read-LocalText $taskInfo) -notmatch '(?m)^Arknights\s*$') { throw 'Target is not the CN Arknights client.' }
    foreach ($taskRow in $Manifest.expectedClientFiles) { Assert-Hash (Get-ChildPath $Root $taskRow.relative) $taskRow.sha256 }
    $taskCompatibility=Read-Property $Manifest 'nativeResourceCompatibility'
    if ($taskCompatibility) {
        $taskResourceCompatibility=Read-Property $taskResourceManifest 'nativeResourceCompatibility'
        if ($taskCompatibility.version -ne 1 -or $taskCompatibility.policy -cne 'native-index-cab-and-logical-name' -or $taskCompatibility.identitySha256 -cnotmatch '^[0-9a-f]{64}$' -or $taskCompatibility.bundleCount -lt 1 -or -not $taskResourceCompatibility -or ($taskCompatibility | ConvertTo-Json -Compress) -cne ($taskResourceCompatibility | ConvertTo-Json -Compress)) { throw 'Unsupported native resource compatibility policy.' }
        if (@($taskResourceManifest.originalClientResources | Where-Object { $_.role -eq 'bundle' }).Count -ne $taskCompatibility.bundleCount) { throw 'Native resource identity count differs.' }
        $taskHotUpdate=Get-ChildPath $Root 'Arknights_Data\PersistentData\Bundles\hot_update_list.json'
        $taskHotInfo=Read-LocalText $taskHotUpdate | ConvertFrom-Json
        if ($taskHotInfo.manifestName -cnotmatch '^[a-f0-9]{32}\.idx$') { throw 'Invalid current CN resource manifest name.' }
        [void](Get-Digest (Get-ChildPath $Root ('Arknights_Data\PersistentData\Bundles\'+$taskHotInfo.manifestName)))
        Write-Host 'CN build verified; current official resources will be checked by native index, CAB identity and logical bundle name at activity load.'
        return
    }
    $taskOriginalResources=@{}
    foreach ($taskRow in @($taskResourceManifest.originalClientResources)) {
        $taskOriginal=Get-ChildPath $Root $taskRow.relative
        if ($taskOriginalResources.ContainsKey($taskOriginal)) { throw 'Duplicate original-client resource path.' }; $taskOriginalResources[$taskOriginal]=$true
        Assert-Hash $taskOriginal $taskRow.sha256
    }
}
function Get-PortableManifest($Manifest,[string]$Root) {
    $taskPortable=Read-Property $Manifest 'portableClientResources'
    $taskDeploymentPortable=Read-Property $taskDeploymentManifest 'portableClientResources'
    if (-not $taskPortable -or -not $taskDeploymentPortable -or $taskPortable.resolverVersion -ne 1 -or $taskPortable.closureValidated -ne $true -or $taskDeploymentPortable.resolverVersion -ne 1 -or $taskDeploymentPortable.closureValidated -ne $true -or $taskDeploymentPortable.manifestSha256 -ne $taskPortable.manifestSha256) { throw 'A closure-validated portable resource resolver v1 package is required.' }
    $taskResourceManifestPath=Get-ChildPath $Root 'resource-manifest.json'
    Assert-Hash $taskResourceManifestPath $taskPortable.manifestSha256
    $taskResources=Read-LocalText $taskResourceManifestPath | ConvertFrom-Json
    if ($taskResources.product -ne 'StrongholdProtocol.PortableClientResources' -or $taskResources.schemaVersion -ne 1 -or $taskResources.resolverVersion -ne 1 -or $taskResources.packageVersion -ne $Manifest.version -or $taskResources.sourceBuildProfileSha256 -cnotmatch '^[0-9a-f]{64}$') { throw 'Portable resource manifest identity/version differs from the package.' }
    $taskRows=@(Get-ResourceRows)
    if ($taskRows.Count -ne @($taskResources.resources).Count) { throw 'Deployment resource closure differs from the tested resource manifest.' }
    foreach ($taskRow in @($taskResources.resources)) {
        $taskMatches=@($taskRows | Where-Object { $_.path -ceq $taskRow.path -and $_.sha256 -ceq $taskRow.sha256 -and $_.size -eq $taskRow.size })
        if ($taskMatches.Count -ne 1) { throw 'Deployment resource closure differs from the tested resource manifest.' }
    }
    return $taskResources
}
function Get-Ownership([string]$Root) {
    $taskReceipt = Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.install.json'
    if (-not (Test-LocalPath $taskReceipt 'Leaf')) { return $null }
    [void](Get-Digest $taskReceipt)
    $taskOwner = Read-LocalText $taskReceipt | ConvertFrom-Json
    if ($taskOwner.product -ne 'StrongholdProtocol.CNActivity' -or $taskOwner.gameRoot -ne $Root -or $taskOwner.proxyTarget -ne (Join-Path $Root 'd3dcompiler_47.dll') -or $taskOwner.originalBackup -ne (Join-Path $Root 'd3dcompiler_47.original.dll') -or $taskOwner.pluginTarget -ne (Join-Path $Root 'plugin\StrongholdProtocolCN\StrongholdCNActivity.dll') -or $taskOwner.configTarget -ne (Join-Path $Root 'plugin\StrongholdProtocolCN\cn-activity.ini') -or $taskOwner.proxyStage -ne (Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.staging.dll') -or $taskOwner.restoreStage -ne (Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.restore.dll')) { throw 'Installation receipt does not own the exact selected paths.' }
    foreach ($taskPair in @(@($taskOwner.proxyTarget,$taskOwner.proxySha256),@($taskOwner.pluginTarget,$taskOwner.pluginSha256),@($taskOwner.originalBackup,$taskOwner.originalSha256))) { Assert-Hash $taskPair[0] $taskPair[1] }
    if (-not (Test-LocalPath $taskOwner.configTarget 'Leaf')) { throw 'Owned installation configuration is missing.' }
    [void](Get-Digest $taskOwner.configTarget)
    return $taskOwner
}
function Get-ResourceTarget([string]$Root,[string]$Relative) {
    $taskResources=Get-LocalPath (Join-Path $Root 'plugin\StrongholdProtocolCN\resources')
    $taskTarget=Get-ChildPath $taskResources $Relative
    if (-not $taskTarget.StartsWith($taskResources+'\client-data\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Overlay resources must stay in the private client-data subtree.' }
    return $taskTarget
}
function Get-ResourceOwner([string]$Root) {
    $taskResourceReceipt=Get-LocalPath (Join-Path $Root 'plugin\StrongholdProtocolCN\deployment-resources.json')
    if (Test-LocalPath ($taskResourceReceipt+'.transaction.json')) { throw 'An interrupted resource transaction exists. Use the persistent backup listed in deployment-resources.json.transaction.json before retrying.' }
    if (-not (Test-LocalPath $taskResourceReceipt 'Leaf')) { return $null }
    [void](Get-Digest $taskResourceReceipt)
    $taskResourceOwner=Read-LocalText $taskResourceReceipt | ConvertFrom-Json
    if ($taskResourceOwner.product -ne 'StrongholdProtocol.ClientResources' -or $taskResourceOwner.gameRoot -ne $Root) { throw 'Overlay resource ownership does not match the selected game.' }
    $taskUnique=@{}
    foreach ($taskRow in @($taskResourceOwner.files)) {
        $taskTarget=Get-ResourceTarget $Root $taskRow.path
        if ($taskUnique.ContainsKey($taskTarget)) { throw 'Duplicate owned overlay resource path.' }; $taskUnique[$taskTarget]=$true
        Assert-Hash $taskTarget $taskRow.sha256
    }
    return $taskResourceOwner
}
function Get-ResourceRows {
    if (Read-Property $taskDeploymentManifest 'resources') { return @($taskDeploymentManifest.resources) }; return @()
}
function Assert-Resources([string]$Root,$Owner) {
    $taskOld=@{};if ($Owner) { foreach ($taskRow in @($Owner.files)) { $taskOld[(Get-ResourceTarget $Root $taskRow.path)]=$taskRow.sha256 } }
    $taskSeen=@{}
    foreach ($taskRow in @(Get-ResourceRows)) {
        $taskTarget=Get-ResourceTarget $Root $taskRow.path
        if ($taskSeen.ContainsKey($taskTarget)) { throw 'Duplicate package overlay resource path.' }; $taskSeen[$taskTarget]=$true
        Assert-Hash (Get-ChildPath (Join-Path $taskPackage 'resources') $taskRow.path) $taskRow.sha256
        if ((Test-LocalPath $taskTarget) -and -not $taskOld.ContainsKey($taskTarget)) { throw "Unowned overlay resource already exists: $taskTarget" }
    }
}
function Copy-Checked([string]$Source,[string]$Target,[string]$Digest) {
    Assert-Hash $Source $Digest; [void](Get-LocalPath $Target)
    $taskParent=[StrongholdDeployment.NativeFile]::Parent($Target)
    $taskStage=$taskParent+'\.spcn-'+[Guid]::NewGuid().ToString('N')+'.tmp'
    [StrongholdDeployment.NativeFile]::MakeDirectory($taskParent)
    try {
        [StrongholdDeployment.NativeFile]::Copy($Source,$taskStage); Assert-Hash $taskStage $Digest
        [StrongholdDeployment.NativeFile]::Commit($taskStage,$Target)
        Assert-Hash $Target $Digest
    } finally { if (Test-LocalPath $taskStage 'Leaf') { [StrongholdDeployment.NativeFile]::Delete($taskStage) } }
}
function Install-Resources([string]$Root,$Owner,[string]$Backup) {
    $taskRows=@(Get-ResourceRows);$taskResourceReceipt=Join-Path $Root 'plugin\StrongholdProtocolCN\deployment-resources.json'
    $taskTransaction=$taskResourceReceipt+'.transaction.json'
    if (-not $Owner -and $taskRows.Count -eq 0) { return }
    Assert-Resources $Root $Owner
    Write-Json $taskTransaction ([ordered]@{product='StrongholdProtocol.ClientResources.Transaction';gameRoot=$Root;backupFolder=$Backup;previous=$Owner;next=$taskRows})
    try {
        $taskNew=@{}
        foreach ($taskRow in $taskRows) {
            $taskTarget=Get-ResourceTarget $Root $taskRow.path; $taskNew[$taskTarget]=$true
            Copy-Checked (Get-ChildPath (Join-Path $taskPackage 'resources') $taskRow.path) $taskTarget $taskRow.sha256
        }
        if ($Owner) { foreach ($taskRow in @($Owner.files)) { $taskTarget=Get-ResourceTarget $Root $taskRow.path; if (-not $taskNew.ContainsKey($taskTarget)) { Assert-Hash $taskTarget $taskRow.sha256; [StrongholdDeployment.NativeFile]::Delete($taskTarget) } } }
        Write-Json $taskResourceReceipt ([ordered]@{product='StrongholdProtocol.ClientResources';gameRoot=$Root;version=$taskManifest.version;files=$taskRows;backupFolder=$Backup})
        [StrongholdDeployment.NativeFile]::Delete($taskTransaction)
    } catch {
        $taskResourceFailure=$_
        try {
            $taskOld=@{}; if ($Owner) { foreach ($taskRow in @($Owner.files)) { $taskOld[(Get-ResourceTarget $Root $taskRow.path)]=$true } }
            foreach ($taskRow in $taskRows) { $taskTarget=Get-ResourceTarget $Root $taskRow.path; if (-not $taskOld.ContainsKey($taskTarget) -and (Test-LocalPath $taskTarget 'Leaf')) { Assert-Hash $taskTarget $taskRow.sha256; [StrongholdDeployment.NativeFile]::Delete($taskTarget) } }
            if ($Owner) {
                foreach ($taskRow in @($Owner.files)) { Copy-Checked (Get-ChildPath (Join-Path $Backup 'previous-resources') $taskRow.path) (Get-ResourceTarget $Root $taskRow.path) $taskRow.sha256 }
                Write-Atomic $taskResourceReceipt ([StrongholdDeployment.NativeFile]::ReadBytes((Join-Path $Backup 'previous-resources.json')))
            } elseif (Test-LocalPath $taskResourceReceipt 'Leaf') { [StrongholdDeployment.NativeFile]::Delete($taskResourceReceipt) }
            [StrongholdDeployment.NativeFile]::Delete($taskTransaction)
        } catch { Write-Warning "Resource rollback needs recovery from $Backup; the transaction record was retained." }
        throw $taskResourceFailure
    }
}
function Remove-Resources([string]$Root,$Owner) {
    if (-not $Owner) { return }
    foreach ($taskRow in @($Owner.files)) { $taskTarget=Get-ResourceTarget $Root $taskRow.path; Assert-Hash $taskTarget $taskRow.sha256; [StrongholdDeployment.NativeFile]::Delete($taskTarget) }
    [StrongholdDeployment.NativeFile]::Delete((Join-Path $Root 'plugin\StrongholdProtocolCN\deployment-resources.json'))
}
function Find-GameRoot($Settings) {
    $taskCandidates = @()
    if ((Read-Property $Settings 'gameRoot') -and (Test-LocalPath (Join-Path $Settings.gameRoot 'Arknights.exe') 'Leaf')) { return (Get-LocalPath $Settings.gameRoot) }
    $taskCandidates += @((Join-Path $PSScriptRoot '..\..\Arknights Game'),'D:\Hypergryph Launcher\games\Arknights Game')
    if ($env:ProgramFiles) { $taskCandidates += (Join-Path $env:ProgramFiles 'Hypergryph Launcher\games\Arknights Game') }
    foreach ($taskRegistry in @('HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*','HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*','HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*')) {
        foreach ($taskApp in @(Get-ItemProperty -Path $taskRegistry -ErrorAction SilentlyContinue)) {
            if ((Read-Property $taskApp 'DisplayName') -match 'Arknights|明日方舟' -and (Read-Property $taskApp 'InstallLocation')) { $taskCandidates += $taskApp.InstallLocation }
        }
    }
    $taskValid = @($taskCandidates | Select-Object -Unique | Where-Object { Test-LocalPath (Join-Path $_ 'Arknights.exe') 'Leaf' })
    if ($taskValid.Count -eq 1) { return Get-LocalPath $taskValid[0] }
    if ($NonInteractive) { throw 'GameRoot was not uniquely discovered; pass -GameRoot.' }
    return Get-LocalPath ((Read-Host 'Paste the CN game directory containing Arknights.exe').Trim('"'))
}
function New-Backup([string]$Root,$Owner) {
    $taskDirectory = Join-Path $taskState ('backups\'+[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N'))
    [void](Get-LocalPath $taskDirectory); [StrongholdDeployment.NativeFile]::MakeDirectory($taskDirectory)
    $taskRecord = [ordered]@{ product='StrongholdProtocol.ClientBackup'; gameRoot=$Root; createdUtc=[DateTime]::UtcNow.ToString('O'); action=$Action; packageVersion=$taskManifest.version; previousVersion=$null; localOnlyConfigurationBackup=$true }
    if ($Owner) {
        $taskRecord.previousVersion=$Owner.version
        [StrongholdDeployment.NativeFile]::Copy((Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.install.json'),(Join-Path $taskDirectory 'previous-install.json'))
        # Exact old settings stay only in this machine's persistent backup.
        # Replacement and rollback configs always clear the operator token.
        [StrongholdDeployment.NativeFile]::Copy($Owner.configTarget,(Join-Path $taskDirectory 'previous-cn-activity.ini'))
    }
    if ($taskResourcesOwner) {
        [StrongholdDeployment.NativeFile]::Copy((Join-Path $Root 'plugin\StrongholdProtocolCN\deployment-resources.json'),(Join-Path $taskDirectory 'previous-resources.json'))
        foreach ($taskRow in @($taskResourcesOwner.files)) { Copy-Checked (Get-ResourceTarget $Root $taskRow.path) (Get-ChildPath (Join-Path $taskDirectory 'previous-resources') $taskRow.path) $taskRow.sha256 }
    }
    Write-Json (Join-Path $taskDirectory 'backup.json') $taskRecord
    Write-Json (Join-Path $taskState 'last-backup.json') ([ordered]@{backupFolder=$taskDirectory;gameRoot=$Root;action=$Action;createdUtc=[DateTime]::UtcNow.ToString('O')})
    return $taskDirectory
}
function Save-Config([string]$Root,[string]$Origin,[byte[]]$Bytes) {
    $taskOwner = Get-Ownership $Root
    if (-not $taskOwner) { throw 'An owned installation is required to configure the client.' }
    $taskOldBytes = [StrongholdDeployment.NativeFile]::ReadBytes($taskOwner.configTarget)
    $taskReceiptPath=Join-Path $Root 'd3dcompiler_47.StrongholdProtocolCN.install.json'
    $taskOldReceiptBytes=[StrongholdDeployment.NativeFile]::ReadBytes($taskReceiptPath)
    $taskSavedSettings=Join-Path $taskState 'settings.json'
    $taskOldSettingsBytes=$null
    if (Test-LocalPath $taskSavedSettings 'Leaf') { $taskOldSettingsBytes=[StrongholdDeployment.NativeFile]::ReadBytes($taskSavedSettings) }
    try {
        Write-Atomic $taskOwner.configTarget $Bytes
        $taskOwner.configSha256 = Get-Digest $taskOwner.configTarget
        Write-Json $taskReceiptPath $taskOwner
        Write-Json $taskSavedSettings ([ordered]@{gameRoot=$Root;endpoint=$Origin;version=$taskOwner.version})
    } catch {
        Write-Atomic $taskOwner.configTarget $taskOldBytes; Write-Atomic $taskReceiptPath $taskOldReceiptBytes
        if ($null -ne $taskOldSettingsBytes) { Write-Atomic $taskSavedSettings $taskOldSettingsBytes }
        throw
    }
}

$taskDeployment = Get-LocalPath $PSScriptRoot
$taskDeploymentManifestPath = Join-Path $taskDeployment 'deployment-client-manifest.json'
[void](Get-Digest $taskDeploymentManifestPath)
$taskDeploymentManifest = Read-LocalText $taskDeploymentManifestPath | ConvertFrom-Json
if ($taskDeploymentManifest.schemaVersion -ne 1 -or $taskDeploymentManifest.product -ne 'StrongholdProtocol.ClientDeployment') { throw 'Unsupported client deployment manifest.' }
$taskEngine = Get-ChildPath $taskDeployment 'engine\Install-CNActivity.ps1'
Assert-Hash $taskEngine $taskDeploymentManifest.installerSha256
if (-not $PackageRoot) { $PackageRoot = Join-Path $taskDeployment 'payload' }
$taskPackage = Get-LocalPath $PackageRoot
$taskManifestPath = Join-Path $taskPackage 'package-manifest.json'
Assert-Hash $taskManifestPath $taskDeploymentManifest.packageManifestSha256
if (Read-Property $taskDeploymentManifest 'files') {
    foreach ($taskFile in $taskDeploymentManifest.files) { Assert-Hash (Get-ChildPath $taskDeployment $taskFile.path) $taskFile.sha256 }
}
$taskManifest = Read-LocalText $taskManifestPath | ConvertFrom-Json
Assert-Package $taskManifest $taskPackage
$taskResourceManifest=Get-PortableManifest $taskManifest $taskPackage
if (-not $StateRoot) {
    if (-not $env:LOCALAPPDATA) { throw 'LOCALAPPDATA is unavailable; use an absolute persistent -StateRoot.' }
    $StateRoot = Join-Path $env:LOCALAPPDATA 'StrongholdProtocol\ClientDeployment'
}
$taskState = Get-LocalPath $StateRoot
if ($taskState -eq $taskDeployment -or $taskState.StartsWith($taskDeployment+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Persistent state/backups must be outside the deployment extraction folder.' }
$taskSettingsPath = Join-Path $taskState 'settings.json'; $taskSettings=$null
if (Test-LocalPath $taskSettingsPath 'Leaf') { [void](Get-Digest $taskSettingsPath); $taskSettings=Read-LocalText $taskSettingsPath | ConvertFrom-Json }
if (-not $GameRoot) { $GameRoot = Find-GameRoot $taskSettings }
$taskRoot = Get-LocalPath $GameRoot
if (-not (Test-LocalPath $taskRoot 'Container')) { throw 'The selected game directory does not exist.' }
Assert-InstallerPathLengths $taskRoot $taskPackage $taskState
$taskOwner = Get-Ownership $taskRoot
$taskResourcesOwner = Get-ResourceOwner $taskRoot
if ($Action -ne 'Uninstall') { Assert-Client $taskManifest $taskRoot }
if ($Action -in @('Install','Verify')) { Assert-Resources $taskRoot $taskResourcesOwner }
if ($Action -eq 'Verify') {
    Invoke-InstallerEngine -GameRoot $taskRoot -PackageRoot $taskPackage -VerifyOnly
    Write-Output ("Verified package {0} and exact CN build. No client files changed." -f $taskManifest.version)
    return
}
if ($Action -ne 'Uninstall') {
    if (-not $Endpoint -and (Read-Property $taskSettings 'endpoint')) { $Endpoint=$taskSettings.endpoint }
    if (-not $Endpoint) { if ($NonInteractive) { throw 'Pass -Endpoint for the independent server.' }; $Endpoint=Read-Host 'Independent server HTTP/HTTPS origin (example https://game.example.com)' }
    $taskOrigin=Get-Origin $Endpoint
    $taskOriginHost=([regex]::Match($taskOrigin,'^https?://([^:]+)')).Groups[1].Value
    $taskParticipantProfile=1
    if ($taskOriginHost -ceq 'localhost' -or $taskOriginHost.StartsWith('127.')) { $taskParticipantProfile=0 }
    if ((Get-ServerPublicMode $taskOrigin) -eq $true) { $taskParticipantProfile=1 }
    $taskConfigBytes=[Text.UTF8Encoding]::new($false).GetBytes("[listener]`r`nmode=listen`r`n`r`n[backend]`r`nendpoint=$taskOrigin`r`nparticipant_profile=$taskParticipantProfile`r`naccess_token=`r`n")
}
Assert-Closed $taskRoot
[StrongholdDeployment.NativeFile]::MakeDirectory($taskState)
$taskBackup=New-Backup $taskRoot $taskOwner
if ($Action -eq 'Uninstall') {
    Invoke-InstallerEngine -GameRoot $taskRoot -Uninstall
    Remove-Resources $taskRoot $taskResourcesOwner
    Write-Output "Restored the verified original. Persistent backups: $taskBackup"
    return
}
if ($Action -eq 'Configure') {
    if (-not $taskOwner -or $taskOwner.pluginSha256 -ne $taskManifest.pluginSha256 -or $taskOwner.proxySha256 -ne $taskManifest.proxySha256) { throw 'Install/upgrade this package before configuring it.' }
    Save-Config $taskRoot $taskOrigin $taskConfigBytes
    Write-Output "Configured independent server $taskOrigin; participant_profile=$taskParticipantProfile and operator access_token is empty. Applies at next normal game launch."
    return
}
$taskPreviousPackage=$null; $taskChangedInstallation=$false
try {
    if ($taskOwner -and ($taskOwner.pluginSha256 -ne $taskManifest.pluginSha256 -or $taskOwner.proxySha256 -ne $taskManifest.proxySha256)) {
        if ($taskOwner.originalSha256 -ne $taskManifest.originalSha256) { throw 'Owned installation targets a different original library; refusing upgrade.' }
        $taskPreviousManifest=Get-LocalPath (Join-Path ([string]$taskOwner.backupFolder) 'package-manifest.json')
        [void](Get-Digest $taskPreviousManifest)
        $taskPrevious=Read-LocalText $taskPreviousManifest | ConvertFrom-Json
        if ($taskPrevious.version -ne $taskOwner.version -or $taskPrevious.testsRun -ne $true -or $taskPrevious.compilerProxyTests -ne 'passed' -or $taskPrevious.proxySha256 -ne $taskOwner.proxySha256 -or $taskPrevious.pluginSha256 -ne $taskOwner.pluginSha256 -or $taskPrevious.originalSha256 -ne $taskOwner.originalSha256) { throw 'Matching tested previous-package evidence is required for rollback.' }
        $taskPreviousPackage=Join-Path $taskBackup 'previous-package'
        foreach ($taskDirectory in @('proxy','plugin')) { [StrongholdDeployment.NativeFile]::MakeDirectory((Join-Path $taskPreviousPackage $taskDirectory)) }
        [StrongholdDeployment.NativeFile]::Copy($taskPreviousManifest,(Join-Path $taskPreviousPackage 'package-manifest.json'))
        [StrongholdDeployment.NativeFile]::Copy($taskOwner.proxyTarget,(Join-Path $taskPreviousPackage 'proxy\d3dcompiler_47.dll'))
        [StrongholdDeployment.NativeFile]::Copy($taskOwner.pluginTarget,(Join-Path $taskPreviousPackage 'plugin\StrongholdCNActivity.dll'))
        Assert-Hash (Join-Path $taskPreviousPackage 'proxy\d3dcompiler_47.dll') $taskPrevious.proxySha256
        Assert-Hash (Join-Path $taskPreviousPackage 'plugin\StrongholdCNActivity.dll') $taskPrevious.pluginSha256
        $taskPriorConfigHash=Get-Digest $taskOwner.configTarget
        Invoke-InstallerEngine -GameRoot $taskRoot -Uninstall -VerifyOnly
        $taskChangedInstallation=$true
        Invoke-InstallerEngine -GameRoot $taskRoot -Uninstall
        if (Test-LocalPath $taskOwner.configTarget 'Leaf') { Assert-Hash $taskOwner.configTarget $taskPriorConfigHash; [StrongholdDeployment.NativeFile]::Delete($taskOwner.configTarget) }
    }
    if (-not $taskOwner) { $taskChangedInstallation=$true }
    Invoke-InstallerEngine -GameRoot $taskRoot -PackageRoot $taskPackage -BackupRoot (Join-Path $taskBackup 'original')
    Save-Config $taskRoot $taskOrigin $taskConfigBytes
    Install-Resources $taskRoot $taskResourcesOwner $taskBackup
} catch {
    $taskFailure=$_
    if ($taskChangedInstallation) {
        try {
            Invoke-InstallerEngine -GameRoot $taskRoot -Uninstall
            if ($taskPreviousPackage) {
                $taskConfigTarget=Join-Path $taskRoot 'plugin\StrongholdProtocolCN\cn-activity.ini'
                if (Test-LocalPath $taskConfigTarget) { throw 'Changed configuration remains after failed install; backups retained for manual recovery.' }
                Invoke-InstallerEngine -GameRoot $taskRoot -PackageRoot $taskPreviousPackage -BackupRoot (Join-Path $taskBackup 'rollback-original')
                $taskRestoredConfig=Read-LocalText (Join-Path $taskBackup 'previous-cn-activity.ini')
                $taskRestoredConfig=[regex]::Replace($taskRestoredConfig,'(?im)^\s*access_token\s*=.*$','access_token=')
                $taskRestoredOrigin=([regex]::Match($taskRestoredConfig,'(?im)^\s*endpoint\s*=([^\r\n]+)')).Groups[1].Value
                Save-Config $taskRoot $taskRestoredOrigin ([Text.UTF8Encoding]::new($false).GetBytes($taskRestoredConfig))
            }
        } catch { Write-Warning ("Automatic rollback did not finish: {0}. Recovery evidence: {1}" -f $_.Exception.Message,$taskBackup) }
    }
    throw $taskFailure
}
Write-Output ("Installed package {0}; server={1}; participant_profile={2}; operator access_token empty. Start the server independently, then launch the game normally." -f $taskManifest.version,$taskOrigin,$taskParticipantProfile)
Write-Output "Persistent backup/receipt pointer: $taskState"
