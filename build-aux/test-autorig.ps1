param(
    [string]$Compiler = 'C:/opt/ldc-1.41/bin/ldc2.exe',
    [string]$VisualStudio = 'C:/Program Files/Microsoft Visual Studio/2022/Community',
    [string[]]$TestArguments = @()
)

$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
Push-Location $taskRoot
try {
    Import-Module (Join-Path $VisualStudio 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
    Enter-VsDevShell -VsInstallPath $VisualStudio -SkipAutomaticLocation `
        -DevCmdArguments '-arch=amd64 -host_arch=amd64'
    $taskSources = @(
        'tests/autorig_solver.d', 'tests/autorig_pipeline.d',
        'source/nijigenerate/autorig/json.d', 'source/nijigenerate/autorig/framework.d',
        'source/nijigenerate/autorig/workflow.d', 'source/nijigenerate/autorig/solver/quadratic.d'
    )
    $taskSources += Get-ChildItem 'source/nijigenerate/autorig/deterministic' -Filter '*.d' |
        Where-Object Name -NotIn @('native.d', 'editor.d') | ForEach-Object FullName
    & $Compiler -c -singleobj '-of=out/autorig-solver-tests.obj' -Isource -Itests -Jres $taskSources
    if ($LASTEXITCODE -ne 0) { throw 'AutoRig test compilation failed' }
    $taskCompilerLib = Join-Path (Split-Path (Split-Path $Compiler -Parent) -Parent) 'lib'
    & link /NOLOGO /OUT:out/autorig-solver-tests.exe out/autorig-solver-tests.obj `
        "/LIBPATH:$taskCompilerLib" /LIBPATH:out/osqp/lib phobos2-ldc.lib druntime-ldc.lib `
        ldc_rt.builtins.lib nijigenerate_osqp.lib osqpstatic.lib
    if ($LASTEXITCODE -ne 0) { throw 'AutoRig test linking failed' }
    & './out/autorig-solver-tests.exe' @TestArguments
    if ($LASTEXITCODE -ne 0) { throw 'AutoRig tests failed' }
} finally {
    Pop-Location
}
