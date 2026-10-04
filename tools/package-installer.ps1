param(
    [Parameter(Mandatory=$true)][string]$PackageDirectory,
    [string]$CompilerPath = "$PSScriptRoot/../build/tools/InnoSetup/ISCC.exe",
    [string]$OutputDirectory = "$PSScriptRoot/../build/installers"
)
$ErrorActionPreference = 'Stop'
$taskPackage = (Resolve-Path -LiteralPath $PackageDirectory).Path
$taskManifest = Get-Content -LiteralPath (Join-Path $taskPackage 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($taskManifest.version -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid application version.' }
if (-not (Test-Path -LiteralPath $CompilerPath -PathType Leaf)) { throw 'Run tools/prepare-installer-tools.ps1 first.' }
$taskRuntime = Join-Path $taskPackage 'vc_redist.x64.exe'
if ([version](Get-Item -LiteralPath $taskRuntime).VersionInfo.ProductVersion -lt [version]'14.50.35719.0') { throw 'Repackage with the prepared Microsoft runtime.' }
$taskPrefix = $taskPackage.TrimEnd('\') + '\'
foreach ($taskEntry in $taskManifest.files) {
    $taskFile = [IO.Path]::GetFullPath((Join-Path $taskPackage $taskEntry.path))
    if (-not $taskFile.StartsWith($taskPrefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Manifest path escapes package directory.' }
    if ((Get-FileHash -LiteralPath $taskFile -Algorithm SHA256).Hash -ne $taskEntry.sha256) { throw "Package checksum mismatch: $($taskEntry.path)" }
}
$taskOutput = (New-Item -ItemType Directory -Path $OutputDirectory -Force).FullName
$taskName = "PixelWorkshop-$($taskManifest.version)-rc1-windows-x64-setup"
$taskInstaller = Join-Path $taskOutput ($taskName + '.exe')
if (Test-Path -LiteralPath $taskInstaller) { throw 'Preserve the existing installer; choose another output directory.' }
& $CompilerPath '--quiet-progress' "--define=PackageDirectory=$taskPackage" "--define=AppVersion=$($taskManifest.version)" "--output-dir=$taskOutput" "--output-filename=$taskName" "$PSScriptRoot/installer/pixel-workshop.iss"
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
((Get-FileHash -LiteralPath $taskInstaller -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $taskInstaller)) | Set-Content -LiteralPath ($taskInstaller+'.sha256') -Encoding ASCII
Write-Output "Installer: $taskInstaller"
