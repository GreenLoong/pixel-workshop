param([Parameter(Mandatory)][string]$CacheDirectory)
$ErrorActionPreference='Stop'
$taskCache=New-Item -ItemType Directory -Force -Path $CacheDirectory
$taskArchive=Join-Path $taskCache.FullName 'opencv-4.13.0-windows.exe'
$taskExpected='f0e98c302464d6860777a7015065e11b9b271b5394e6ba92663f0cf1fc303f2c'
if(-not (Test-Path -LiteralPath $taskArchive)) {
    Invoke-WebRequest 'https://github.com/opencv/opencv/releases/download/4.13.0/opencv-4.13.0-windows.exe' -OutFile $taskArchive
}
if((Get-FileHash -LiteralPath $taskArchive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $taskExpected) {throw 'OpenCV archive checksum mismatch.'}
& 7z x $taskArchive "-o$($taskCache.FullName)" -y | Out-Null
if($LASTEXITCODE -ne 0) {throw 'OpenCV extraction failed.'}
$taskRoot=Join-Path $taskCache.FullName 'opencv/build'
if(-not (Test-Path -LiteralPath (Join-Path $taskRoot 'OpenCVConfig.cmake'))) {throw 'OpenCV configuration missing.'}
"OPENCV_ROOT=$taskRoot" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
Join-Path $taskRoot 'x64/vc16/bin' | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
