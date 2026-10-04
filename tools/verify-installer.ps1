param(
    [Parameter(Mandatory=$true)][string]$Installer,
    [string]$OutputDirectory = "$PSScriptRoot/../build/installer-check-$(Get-Date -Format yyyyMMdd-HHmmss)"
)
$ErrorActionPreference = 'Stop'
$taskInstaller = (Resolve-Path -LiteralPath $Installer).Path
$taskHash = (Get-Content -LiteralPath ($taskInstaller + '.sha256')).Split(' ')[0]
if ((Get-FileHash -LiteralPath $taskInstaller -Algorithm SHA256).Hash -ne $taskHash) { throw 'Installer checksum mismatch.' }
# 验证真实安装包；不覆盖已有用户安装或已有快捷方式。
$taskUninstallKey = '{297FDF24-8CA6-4D7C-98BA-B4533A861BDB}_is1'
foreach ($taskKey in @("HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/$taskUninstallKey", "HKCU:/Software/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall/$taskUninstallKey")) {
    if (Test-Path -LiteralPath $taskKey) { throw 'An existing Pixel Workshop installation must not be changed by installer verification.' }
}
$taskShortcutDirectory = Join-Path ([Environment]::GetFolderPath('Programs')) 'Pixel Workshop'
if (Test-Path -LiteralPath $taskShortcutDirectory) { throw 'Existing Pixel Workshop shortcuts must not be changed by installer verification.' }
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new installer verification directory.' }
$taskRoot = (New-Item -ItemType Directory -Path $OutputDirectory).FullName
$taskTemporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
$taskTemporary = Join-Path $taskTemporaryRoot ('pixel-workshop-install-check-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $taskTemporary | Out-Null
$taskApplication = Join-Path $taskTemporary 'installed application'
$taskSetup = Join-Path $taskTemporary 'setup.exe'
Copy-Item -LiteralPath $taskInstaller -Destination $taskSetup

function Invoke-SetupProcess([string]$Executable, [string]$Arguments) {
    $taskProcess = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    try {
        if (-not $taskProcess.WaitForExit(180000)) { $taskProcess.Kill(); throw 'Installer verification timed out.' }
        if ($taskProcess.ExitCode -ne 0) { throw "Setup failed with exit $($taskProcess.ExitCode); see logs in $taskRoot" }
    } finally { $taskProcess.Dispose() }
}

try {
    $taskArguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /LANG=chinesesimplified /TASKS="" /DIR="' + $taskApplication + '"'
    Invoke-SetupProcess $taskSetup ($taskArguments + ' /LOG="' + (Join-Path $taskRoot 'install.log') + '"')
    $taskShortcut = Join-Path $taskShortcutDirectory 'Pixel Workshop.lnk'
    if (-not (Test-Path -LiteralPath $taskShortcut)) { throw 'Start menu shortcut missing.' }
    $taskShell = New-Object -ComObject WScript.Shell
    if ($taskShell.CreateShortcut($taskShortcut).TargetPath -ne (Join-Path $taskApplication 'ImageBatchTool.exe')) { throw 'Incorrect start menu shortcut target.' }
    & "$PSScriptRoot/verify-package.ps1" -PackageDirectory $taskApplication
    $taskInstalledManifest = Get-Content -LiteralPath (Join-Path $taskApplication 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $taskVerification = Get-Content -LiteralPath (Join-Path $taskApplication 'verification-result.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    Remove-Item -LiteralPath (Join-Path $taskApplication 'verification-result.json')
    # 重装应更新已登记的应用，而不是创建第二份卸载记录。
    Invoke-SetupProcess $taskSetup ($taskArguments + ' /LOG="' + (Join-Path $taskRoot 'reinstall.log') + '"')
    & "$PSScriptRoot/verify-package.ps1" -PackageDirectory $taskApplication
    Remove-Item -LiteralPath (Join-Path $taskApplication 'verification-result.json')
    # 卸载只清理安装器管理的文件，用户后来保存的文件必须保留。
    $taskUserFile = Join-Path $taskApplication 'user-file-preservation.txt'
    [IO.File]::WriteAllText($taskUserFile, 'preserve user-created files')
    $taskUninstaller = Join-Path $taskApplication 'unins000.exe'
    Invoke-SetupProcess $taskUninstaller ('/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /LOG="' + (Join-Path $taskRoot 'uninstall.log') + '"')
    if (Test-Path -LiteralPath (Join-Path $taskApplication 'ImageBatchTool.exe')) { throw 'Installed executable was not removed.' }
    if (Test-Path -LiteralPath $taskShortcut) { throw 'Installed shortcut was not removed.' }
    if (-not (Test-Path -LiteralPath $taskUserFile)) { throw 'User-created file was incorrectly removed.' }
    foreach ($taskKey in @("HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/$taskUninstallKey", "HKCU:/Software/WOW6432Node/Microsoft/Windows/CurrentVersion/Uninstall/$taskUninstallKey")) {
        if (Test-Path -LiteralPath $taskKey) { throw 'Uninstall registry entry was not removed.' }
    }
    $taskReport = [ordered]@{version=$taskInstalledManifest.version;gitCommit=$taskInstalledManifest.gitCommit;installerSha256=$taskHash;verifiedAt=[DateTime]::UtcNow.ToString('o');install='passed';installedPackage=$taskVerification;shortcut='passed';reinstall='passed';uninstall='passed';userFilePreservation='passed';manualInteraction='pending'}
    $taskReport | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskRoot 'installer-verification.json') -Encoding UTF8
    Write-Output "PASS: install, startup, processing, shortcuts, reinstall, uninstall and user-file preservation. Report: $taskRoot"
} finally {
    $taskResolved = [IO.Path]::GetFullPath($taskTemporary)
    if (-not $taskResolved.StartsWith($taskTemporaryRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Installer temporary path escaped the temporary directory.' }
    if (Test-Path -LiteralPath (Join-Path $taskApplication 'ImageBatchTool.exe')) {
        Write-Warning "Verification did not finish uninstalling; preserve $taskResolved for diagnosis."
    } else {
        Remove-Item -LiteralPath $taskResolved -Recurse -Force
    }
}
