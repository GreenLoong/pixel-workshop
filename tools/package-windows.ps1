param(
    [string]$BuildDirectory = "$PSScriptRoot/../ImageBatchTool/build/release-clean",
    [string]$QtDirectory = 'D:/Qt/6.11.1/msvc2022_64',
    [string]$OpenCVDll = 'D:/Tools/OpenCV/opencv/build/x64/vc16/bin/opencv_world4130.dll',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$taskExecutable = Join-Path $BuildDirectory 'ImageBatchTool.exe'
$taskDeployTool = Join-Path $QtDirectory 'bin/windeployqt.exe'
foreach ($taskFile in @($taskExecutable, $taskDeployTool, $OpenCVDll)) {
    if (-not (Test-Path -LiteralPath $taskFile -PathType Leaf)) { throw "File missing: $taskFile" }
}
if (-not (Select-String -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt') -Pattern '^CMAKE_BUILD_TYPE:STRING=Release$' -Quiet)) {
    throw 'Use a Release build directory.'
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "../build/windows-package-$(Get-Date -Format yyyyMMdd-HHmmss)"
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'Choose a new output directory to preserve previous packages.' }
$taskPackage = New-Item -ItemType Directory -Path $OutputDirectory
Copy-Item -LiteralPath $taskExecutable -Destination $taskPackage.FullName
& $taskDeployTool --release --no-translations --no-compiler-runtime --dir $taskPackage.FullName (Join-Path $taskPackage.FullName 'ImageBatchTool.exe')
if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
Copy-Item -LiteralPath $OpenCVDll -Destination $taskPackage.FullName
foreach ($taskFile in @('Qt6Core.dll','Qt6Gui.dll','Qt6Widgets.dll','platforms/qwindows.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $taskPackage.FullName $taskFile))) { throw "Deployment missing: $taskFile" }
}
[IO.File]::WriteAllText((Join-Path $taskPackage.FullName 'qt.conf'), "[Paths]`nPlugins=.`n", [Text.UTF8Encoding]::new($false))
Copy-Item -LiteralPath "$PSScriptRoot/../docs/Windows构建与交付.md" -Destination (Join-Path $taskPackage.FullName '使用说明.md')
Compress-Archive -LiteralPath $taskPackage.FullName -DestinationPath ($taskPackage.FullName + '.zip')
Write-Output "Package: $($taskPackage.FullName)"
Write-Output "Archive: $($taskPackage.FullName).zip"
