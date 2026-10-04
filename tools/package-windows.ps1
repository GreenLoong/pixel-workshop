param(
    [string]$BuildDirectory = "$PSScriptRoot/../ImageBatchTool/build/release-clean",
    [string]$QtDirectory = 'D:/Qt/6.11.1/msvc2022_64',
    [string]$OpenCVDll = 'D:/Tools/OpenCV/opencv/build/x64/vc16/bin/opencv_world4130.dll',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$taskExecutable = Join-Path $BuildDirectory 'ImageBatchTool.exe'
$taskChecker = Join-Path $BuildDirectory 'PixelWorkshopCheck.exe'
$taskDeployTool = Join-Path $QtDirectory 'bin/windeployqt.exe'
foreach ($taskFile in @($taskExecutable, $taskChecker, $taskDeployTool, $OpenCVDll)) {
    if (-not (Test-Path -LiteralPath $taskFile -PathType Leaf)) { throw "File missing: $taskFile" }
}
if (-not (Select-String -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Pattern '^CMAKE_BUILD_TYPE:STRING=Release$' -Quiet)) {
    throw 'Use a Release build directory.'
}
$taskVersion = (Get-Item -LiteralPath $taskExecutable).VersionInfo.ProductVersion
if ($taskVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Build the executable with its version resource first.' }
$taskCommit = & git -C "$PSScriptRoot/.." rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine source revision.' }
$taskDirty = & git -C "$PSScriptRoot/.." status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or $taskDirty) { throw 'Commit source changes before creating a delivery package.' }
$taskQtVersion = & (Join-Path $QtDirectory 'bin/qmake.exe') -query QT_VERSION
if ($LASTEXITCODE -ne 0) { throw 'Cannot determine Qt version.' }
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "../build/pixel-workshop-v$taskVersion-windows-x64-$(Get-Date -Format yyyyMMdd-HHmmss)"
}
if ((Test-Path -LiteralPath $OutputDirectory) -or (Test-Path -LiteralPath ($OutputDirectory+'.zip'))) { throw 'Choose a new output directory to preserve previous packages.' }
$taskPackage = New-Item -ItemType Directory -Path $OutputDirectory
Copy-Item -LiteralPath $taskExecutable -Destination $taskPackage.FullName
Copy-Item -LiteralPath $taskChecker -Destination $taskPackage.FullName
$taskOldVC = $env:VCINSTALLDIR
try {
    # 普通 PowerShell 中通过微软工具定位运行库；开发者环境直接沿用。
    if (-not $env:VCINSTALLDIR) {
        $taskVsWhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio/Installer/vswhere.exe'
        $taskVS = & $taskVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or -not $taskVS) { throw 'Run packaging from an MSVC x64 developer environment.' }
        $env:VCINSTALLDIR = Join-Path $taskVS 'VC/'
    }
    & $taskDeployTool --release --no-translations --compiler-runtime --dir $taskPackage.FullName (Join-Path $taskPackage.FullName 'ImageBatchTool.exe') (Join-Path $taskPackage.FullName 'PixelWorkshopCheck.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
} finally {
    $env:VCINSTALLDIR = $taskOldVC
}
Copy-Item -LiteralPath $OpenCVDll -Destination $taskPackage.FullName
foreach ($taskFile in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','platforms/qwindows.dll','imageformats/qjpeg.dll','vc_redist.x64.exe')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskPackage.FullName $taskFile))) { throw "Deployment missing: $taskFile" }
}
[IO.File]::WriteAllText((Join-Path $taskPackage.FullName 'qt.conf'), "[Paths]`nPlugins=.`n", [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath "$PSScriptRoot/../docs/Windows构建与交付.md" -Destination (Join-Path $taskPackage.FullName '使用说明.md')
Copy-Item -LiteralPath "$PSScriptRoot/../docs/另一台电脑验收.md" -Destination $taskPackage.FullName
Copy-Item -LiteralPath "$PSScriptRoot/../docs/人像分割算法替换.md" -Destination $taskPackage.FullName
Copy-Item -LiteralPath "$PSScriptRoot/verify-package.ps1" -Destination $taskPackage.FullName
New-Item -ItemType Directory -Path (Join-Path $taskPackage.FullName 'licenses') | Out-Null
Copy-Item -LiteralPath "$PSScriptRoot/../ImageBatchTool/resources/models/PPHumanSeg-LICENSE.txt" -Destination (Join-Path $taskPackage.FullName 'licenses')
Copy-Item -LiteralPath "$PSScriptRoot/../ImageBatchTool/resources/models/PPHumanSeg-NOTICE.txt" -Destination (Join-Path $taskPackage.FullName 'licenses')
Copy-Item -LiteralPath "$PSScriptRoot/../ImageBatchTool/resources/licenses/Qt-LICENSE.txt" -Destination (Join-Path $taskPackage.FullName 'licenses')
# 官方 OpenCV 压缩包的许可证位于 opencv/build 的上一层。
$taskOpenCVRoot = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $OpenCVDll) '../../../..'))
Copy-Item -LiteralPath (Join-Path $taskOpenCVRoot 'LICENSE.txt') -Destination (Join-Path $taskPackage.FullName 'licenses/OpenCV-LICENSE.txt')
$taskPrefix = $taskPackage.FullName.TrimEnd([char[]]'\/') + [IO.Path]::DirectorySeparatorChar
$taskFiles = @(Get-ChildItem -LiteralPath $taskPackage.FullName -File -Recurse | Sort-Object FullName | ForEach-Object {
    [ordered]@{path=$_.FullName.Substring($taskPrefix.Length).Replace('\','/');size=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}
})
$taskManifest = [ordered]@{schemaVersion=1;version=$taskVersion;gitCommit=$taskCommit;qtVersion=$taskQtVersion;openCVDll=(Split-Path -Leaf $OpenCVDll);createdUtc=[DateTime]::UtcNow.ToString('o');files=$taskFiles}
$taskManifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $taskPackage.FullName 'manifest.json') -Encoding UTF8
Compress-Archive -LiteralPath $taskPackage.FullName -DestinationPath ($taskPackage.FullName + '.zip')
$taskArchive = $taskPackage.FullName + '.zip'
((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + (Split-Path -Leaf $taskArchive)) | Set-Content -LiteralPath ($taskArchive+'.sha256') -Encoding ASCII
Write-Output "Package: $($taskPackage.FullName)"
Write-Output "Archive: $($taskPackage.FullName).zip"
