param([string]$CacheDirectory = "$PSScriptRoot/../build/tools")
$ErrorActionPreference = 'Stop'
$taskCache = (New-Item -ItemType Directory -Path $CacheDirectory -Force).FullName
$taskCompilerSetup = Join-Path $taskCache 'innosetup-7.1.0-x64.exe'
$taskCompilerHash = '0362A383ED217D4C4239B5933866DD96D3EB2102737DA92F80F6057A4B40DF2F'
if (-not (Test-Path -LiteralPath $taskCompilerSetup)) {
    Invoke-WebRequest -Uri 'https://github.com/jrsoftware/issrc/releases/download/is-7_1_0/innosetup-7.1.0-x64.exe' -OutFile $taskCompilerSetup
}
if ((Get-FileHash -LiteralPath $taskCompilerSetup -Algorithm SHA256).Hash -ne $taskCompilerHash) { throw 'Inno Setup archive checksum mismatch.' }
$taskSignature = Get-AuthenticodeSignature -LiteralPath $taskCompilerSetup
if ($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Pyrsys') { throw 'Inno Setup publisher signature invalid.' }
$taskCompilerDirectory = Join-Path $taskCache 'InnoSetup'
$taskCompiler = Join-Path $taskCompilerDirectory 'ISCC.exe'
if (-not (Test-Path -LiteralPath $taskCompiler)) {
    # 官方便携模式不写入编译器的安装注册表或快捷方式；从临时短路径启动。
    $taskTempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    $taskTemporary = Join-Path $taskTempRoot ('pixel-workshop-inno-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $taskTemporary | Out-Null
    try {
        $taskSetup = Join-Path $taskTemporary 'compiler-setup.exe'
        $taskExtracted = Join-Path $taskTemporary 'compiler'
        Copy-Item -LiteralPath $taskCompilerSetup -Destination $taskSetup
        $taskArguments = '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /PORTABLE=1 /DIR="' + $taskExtracted + '" /LOG="' + (Join-Path $taskCache 'compiler-install.log') + '"'
        $taskProcess = Start-Process -FilePath $taskSetup -ArgumentList $taskArguments -Wait -PassThru -WindowStyle Hidden
        if ($taskProcess.ExitCode -ne 0) { throw "Inno Setup preparation failed: $($taskProcess.ExitCode)" }
        Copy-Item -LiteralPath $taskExtracted -Destination $taskCompilerDirectory -Recurse
    } finally {
        $taskResolved = [IO.Path]::GetFullPath($taskTemporary)
        if (-not $taskResolved.StartsWith($taskTempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Temporary tool path escaped the temporary directory.' }
        Remove-Item -LiteralPath $taskResolved -Recurse -Force
    }
}
$taskRuntime = Join-Path $taskCache 'vc_redist.x64.exe'
if (-not (Test-Path -LiteralPath $taskRuntime)) {
    Invoke-WebRequest -Uri 'https://aka.ms/vc14/vc_redist.x64.exe' -OutFile $taskRuntime
}
$taskSignature = Get-AuthenticodeSignature -LiteralPath $taskRuntime
if ($taskSignature.Status -ne 'Valid' -or $taskSignature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') { throw 'Microsoft runtime publisher signature invalid.' }
$taskRuntimeVersion = [version](Get-Item -LiteralPath $taskRuntime).VersionInfo.ProductVersion
if ($taskRuntimeVersion -lt [version]'14.50.35719.0') { throw 'Use a current Microsoft x64 Visual C++ runtime installer.' }
Write-Host "Inno Setup 7.1.0 and Microsoft runtime $taskRuntimeVersion ready."
[pscustomobject]@{Compiler=$taskCompiler; RuntimeInstaller=$taskRuntime}
