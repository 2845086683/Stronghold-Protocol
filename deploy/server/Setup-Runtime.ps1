param()
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$bundleRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$requirement=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'runtime-requirements.json') -Raw | ConvertFrom-Json
$entry=$requirement.windows
if ($entry.url -cne 'https://www.python.org/ftp/python/3.13.16/python-3.13.16-embed-amd64.zip' -or
    $entry.sha256 -cne '97dae5274cc54867065e8d5a3226e48c35017ed332a0fdb0e27d5b5821961297') {
    throw 'Runtime source pin differs from the reviewed official Python release.'
}
$target=[IO.Path]::GetFullPath((Join-Path $bundleRoot 'runtime\windows'))
if (Test-Path -LiteralPath $target) { throw 'runtime/windows already exists; it will not be overwritten.' }
$temporaryRoot=[IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$workName='StrongholdRuntime-'+[Guid]::NewGuid().ToString('N')
$work=[IO.Path]::GetFullPath((Join-Path $temporaryRoot $workName))
New-Item -ItemType Directory -Path $work | Out-Null
$acl=New-Object Security.AccessControl.DirectorySecurity
$acl.SetAccessRuleProtection($true,$false)
$current=[Security.Principal.WindowsIdentity]::GetCurrent().User
foreach ($identity in @($current,(New-Object Security.Principal.SecurityIdentifier('S-1-5-18')))) {
    $acl.AddAccessRule((New-Object Security.AccessControl.FileSystemAccessRule($identity,'FullControl','ContainerInherit,ObjectInherit','None','Allow')))
}
Set-Acl -LiteralPath $work -AclObject $acl
try {
    $archive=Join-Path $work 'python.zip'
    Invoke-WebRequest -UseBasicParsing -Uri $entry.url -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $entry.sha256) {
        throw 'Official runtime SHA256 mismatch; no executable was run.'
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip=[IO.Compression.ZipFile]::OpenRead($archive)
    $total=[int64]0
    try {
        if ($zip.Entries.Count -gt 1024) { throw 'Unexpected runtime archive entry count.' }
        foreach ($item in $zip.Entries) {
            $total+=$item.Length
            $name=$item.FullName.Replace('/',[IO.Path]::DirectorySeparatorChar)
            $resolved=[IO.Path]::GetFullPath((Join-Path $work $name))
            if ([IO.Path]::IsPathRooted($name) -or !$resolved.StartsWith($work+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or
                (($item.ExternalAttributes -shr 16) -band 61440) -eq 40960 -or $total -gt 268435456) {
                throw 'Unsafe runtime archive layout.'
            }
        }
    } finally { $zip.Dispose() }
    $extracted=Join-Path $work 'extracted'
    [IO.Compression.ZipFile]::ExtractToDirectory($archive,$extracted)
    $signature=Get-AuthenticodeSignature -LiteralPath (Join-Path $extracted 'python.exe')
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'CN=Python Software Foundation(?:,|$)') {
        throw 'Python executable publisher validation failed; no executable was run.'
    }
    if (!(Test-Path -LiteralPath (Join-Path $extracted 'LICENSE.txt') -PathType Leaf)) { throw 'Official Python license is missing.' }
    New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
    Move-Item -LiteralPath $extracted -Destination $target
    Write-Output 'Official embedded runtime installed with its original license. Start-Server.cmd can now run.'
} finally {
    # The only recursive cleanup is this exact generated child of OS temp.
    $verified=[IO.Path]::GetFullPath($work)
    if ($verified.StartsWith($temporaryRoot,[StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($verified) -ceq $workName -and $workName -match '^StrongholdRuntime-[0-9a-f]{32}$') {
        Remove-Item -LiteralPath $verified -Recurse -Force
    }
}
