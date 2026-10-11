param([ValidateSet('msvc', 'clang-cl', 'mingw')][string]$Compiler,
      [ValidateSet('x64', 'arm64')][string]$Architecture = 'x64')
$ErrorActionPreference = 'Stop'
if ($Compiler -eq 'mingw') {
    if ($Architecture -ne 'x64') { throw 'MinGW ARM64 is not an advertised configuration' }
    $taskCandidates = @('C:\msys64\ucrt64\bin', 'C:\msys64\mingw64\bin', 'C:\mingw64\bin',
        'C:\ProgramData\chocolatey\lib\mingw\tools\install\mingw64\bin')
    $taskExisting = Get-Command g++.exe -ErrorAction SilentlyContinue
    if ($taskExisting) { $taskCandidates += [IO.Path]::GetDirectoryName($taskExisting.Source) }
    $taskBin = $taskCandidates | Where-Object { Test-Path "$_\g++.exe" } | Select-Object -First 1
    if (-not $taskBin) { throw 'Runner is missing MinGW-w64 GCC' }
    $taskTarget = & "$taskBin\g++.exe" -dumpmachine
    if ($taskTarget -notmatch '^x86_64-.*mingw') { throw "Unexpected GCC target: $taskTarget" }
    $taskBin | Out-File $env:GITHUB_PATH -Append -Encoding utf8
    "CC=$taskBin\gcc.exe" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    "CXX=$taskBin\g++.exe" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    & "$taskBin\g++.exe" --version
    exit $LASTEXITCODE
}
$taskVswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$taskVs = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $taskVs) { $taskVs = & $taskVswhere -latest -products '*' -property installationPath }
if (-not $taskVs) { throw 'Visual Studio not found' }
$taskBefore = @{}
Get-ChildItem Env: | ForEach-Object { $taskBefore[$_.Name] = $_.Value }
Import-Module "$taskVs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
Enter-VsDevShell -VsInstallPath $taskVs -SkipAutomaticLocation -DevCmdArguments "-arch=$Architecture -host_arch=x64"
Get-ChildItem Env: | Where-Object { $taskBefore[$_.Name] -ne $_.Value } | ForEach-Object {
    "$($_.Name)=$($_.Value)" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
}
if ($Compiler -eq 'clang-cl') {
    $taskClang = "$taskVs\VC\Tools\Llvm\x64\bin\clang-cl.exe"
    if (-not (Test-Path $taskClang)) { $taskClang = (Get-Command clang-cl.exe -ErrorAction Stop).Source }
    "CC=$taskClang" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    "CXX=$taskClang" | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    if ($Architecture -eq 'arm64') {
        'CFLAGS=--target=aarch64-pc-windows-msvc' | Out-File $env:GITHUB_ENV -Append -Encoding utf8
        'CXXFLAGS=--target=aarch64-pc-windows-msvc' | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    }
    & $taskClang --version
} else {
    'CC=cl' | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    'CXX=cl' | Out-File $env:GITHUB_ENV -Append -Encoding utf8
    & cl.exe 2>&1 | Select-Object -First 3
}
