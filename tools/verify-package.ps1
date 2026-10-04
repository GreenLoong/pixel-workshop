param([string]$PackageDirectory=$PSScriptRoot)
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path -LiteralPath $PackageDirectory).Path
$taskManifest=Get-Content -LiteralPath (Join-Path $taskRoot 'manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if($taskManifest.schemaVersion -ne 1) {throw 'Unsupported package manifest.'}
$taskPrefix=$taskRoot.TrimEnd([char[]]'\/')+[IO.Path]::DirectorySeparatorChar
foreach($taskEntry in $taskManifest.files) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskEntry.path))
    if(-not $taskPath.StartsWith($taskPrefix,[StringComparison]::OrdinalIgnoreCase)) {throw 'Manifest path escapes package directory.'}
    if(-not (Test-Path -LiteralPath $taskPath -PathType Leaf)) {throw "Missing package file: $($taskEntry.path)"}
    if((Get-FileHash -LiteralPath $taskPath -Algorithm SHA256).Hash -ne $taskEntry.sha256) {throw "Checksum mismatch: $($taskEntry.path)"}
}
foreach($taskName in @('ImageBatchTool.exe','PixelWorkshopCheck.exe','platforms/qwindows.dll')) {
    if($taskName -notin $taskManifest.files.path) {throw "Required file absent from manifest: $taskName"}
}
$taskOldPath=$env:Path
$taskEnvironment=@{}
foreach($taskName in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_PLATFORM','QT_SCALE_FACTOR')) {
    $taskEnvironment[$taskName]=[Environment]::GetEnvironmentVariable($taskName,'Process')
    [Environment]::SetEnvironmentVariable($taskName,$null,'Process')
}
Push-Location -LiteralPath $taskRoot
try {
    # 子进程只可通过包内 DLL 和 Windows 系统目录获得依赖。
    $env:Path="$env:SystemRoot/System32;$env:SystemRoot;$env:SystemRoot/System32/Wbem"
    foreach($taskCheck in @(
        @{exe='PixelWorkshopCheck.exe';arguments=''},
        @{exe='ImageBatchTool.exe';arguments='--verify-startup'}
    )) {
        $taskStart=[Diagnostics.ProcessStartInfo]::new()
        $taskStart.FileName=Join-Path $taskRoot $taskCheck.exe
        $taskStart.Arguments=$taskCheck.arguments
        $taskStart.WorkingDirectory=$taskRoot
        $taskStart.UseShellExecute=$false
        $taskStart.CreateNoWindow=$true
        $taskStart.WindowStyle=[Diagnostics.ProcessWindowStyle]::Hidden
        $taskProcess=[Diagnostics.Process]::Start($taskStart)
        try {
            if(-not $taskProcess.WaitForExit(30000)) {
                $taskProcess.Kill()
                throw "Verification timed out: $($taskCheck.exe)"
            }
            if($taskProcess.ExitCode -ne 0) {throw "Verification failed: $($taskCheck.exe), exit $($taskProcess.ExitCode)"}
        } finally {$taskProcess.Dispose()}
    }
    $taskReport=[ordered]@{version=$taskManifest.version;gitCommit=$taskManifest.gitCommit;verifiedAt=[DateTime]::UtcNow.ToString('o');fileHashes='passed';imagePipeline='passed';mainWindow='passed';manualInteraction='pending'}
    $taskReport | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $taskRoot 'verification-result.json') -Encoding UTF8
    Write-Output "PASS: package $($taskManifest.version), file integrity, image pipeline and main window startup."
}finally {
    Pop-Location
    $env:Path=$taskOldPath
    foreach($taskName in $taskEnvironment.Keys) {[Environment]::SetEnvironmentVariable($taskName,$taskEnvironment[$taskName],'Process')}
}
