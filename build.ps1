param(
    [string]$Compiler = 'g++',
    [switch]$Test,
    [switch]$DebugBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$core = @('src/levels.cpp', 'src/editor_level.cpp', 'src/portal.cpp', 'src/physics.cpp', 'src/game.cpp', 'src/tutorial.cpp') |
    ForEach-Object { Join-Path $projectRoot $_ }
$flags = @('-std=c++17', '-Wall', '-Wextra', '-Wpedantic', '-I', (Join-Path $projectRoot 'include'))
if ($DebugBuild) { $flags += @('-O0', '-g') } else { $flags += '-O2' }
# LLVM-MinGW otherwise needs runtime DLLs beside the executable or on PATH.
$flags += '-static'
& $Compiler @flags @core (Join-Path $projectRoot 'src/render.cpp') (Join-Path $projectRoot 'src/main.cpp') '-lgdi32' '-luser32' '-lcomdlg32' '-mwindows' '-o' (Join-Path $buildDir 'Por2D.exe')
if ($LASTEXITCODE -ne 0) { throw 'Game build failed.' }
Write-Host "Built $buildDir\Por2D.exe"
if ($Test) {
    & $Compiler @flags @core (Join-Path $projectRoot 'src/render.cpp') (Join-Path $projectRoot 'tests/tests.cpp') (Join-Path $projectRoot 'tests/legacy_bridge.cpp') '-I' (Join-Path $projectRoot 'tests/legacy_stubs') '-o' (Join-Path $buildDir 'por2_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
    & (Join-Path $buildDir 'por2_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Regression tests failed.' }
}
