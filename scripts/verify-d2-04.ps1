param(
    [string]$QtPrefix = 'C:/Software/MSYS2/ucrt64',
    [string]$QmlPrefix = 'D:/Git/Relaxer/phase-c-work/deps/ucrt64',
    [string]$BuildDirectory = '',
    [string]$EvidenceDirectory = '',
    [switch]$Build,
    [switch]$NativeSmoke
)
$ErrorActionPreference = 'Stop'
$sourceRoot = Split-Path $PSScriptRoot -Parent
$taskRoot = Split-Path $sourceRoot -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $taskRoot 'build' }
if (!$EvidenceDirectory) { $EvidenceDirectory = Join-Path $taskRoot 'evidence' }
$testTemp = Join-Path $taskRoot 'temp'
New-Item -ItemType Directory -Force -Path $EvidenceDirectory,$testTemp | Out-Null
$env:PATH = "$QtPrefix/bin;$QmlPrefix/bin;" + $env:PATH
$env:TEMP = $testTemp
$env:TMP = $testTemp
$env:QML_IMPORT_PATH = "$QmlPrefix/share/qt6/qml"
$env:QML2_IMPORT_PATH = $env:QML_IMPORT_PATH
$env:QT_QUICK_BACKEND = 'software'
$env:QT_QPA_PLATFORM = 'offscreen'
$env:RELAXER_SKIP_AUDIO_DEVICE_TESTS = '1'
$env:RELAXER_D203_EVIDENCE = Join-Path $EvidenceDirectory 'd203-interaction'
$env:RELAXER_D204_EVIDENCE = Join-Path $EvidenceDirectory 'search-interaction'
$env:RELAXER_D2_ESCAPE_EVIDENCE = Join-Path $EvidenceDirectory 'escape-interaction'
$targets = @('relaxer_d204_search_fixture','relaxer_lyrics_search_parameters_tests','relaxer_lyrics_search_coordinator_tests','Relaxer','relaxer_d203_fixture','relaxer_lyrics_edit_tests','relaxer_qml_error_feedback_tests',
    'relaxer_lyrics_persistence_tests','relaxer_lyrics_save_work_tests','relaxer_lyrics_replacement_policy_tests',
    'relaxer_playback_facade_tests','relaxer_import_coordinator_tests','relaxer_lyrics_replacement_integration_tests',
    'relaxer_qml_lyrics_workspace_parity_tests','relaxer_qml_search_bridge_tests','relaxer_translation_coordinator_tests')
if ($Build) {
    $configArgs = @('-S',$sourceRoot,'-B',$BuildDirectory,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=ON','-DRELAXER_ENABLE_QML=ON',"-DCMAKE_PREFIX_PATH=$QtPrefix", "-DCMAKE_CXX_COMPILER=$QtPrefix/bin/c++.exe")
    foreach ($name in @('Qml','QmlIntegration','QmlMeta','QmlModels','QmlTools','QmlWorkerScript','Quick','QuickTools','QuickControls2','QuickTemplates2')) {
        $configArgs += "-DQt6${name}_DIR=$QmlPrefix/lib/cmake/Qt6$name"
    }
    & "$QtPrefix/bin/cmake.exe" @configArgs *> (Join-Path $EvidenceDirectory 'configure.log')
    if ($LASTEXITCODE) { throw 'CMake configure failed; see configure.log' }
    & "$QtPrefix/bin/cmake.exe" --build $BuildDirectory --target @targets --parallel 4 *> (Join-Path $EvidenceDirectory 'build.log')
    if ($LASTEXITCODE) { throw 'Build failed; see build.log' }
}
$selection = '^(relaxer_lyrics_search_parameters|relaxer_lyrics_search_coordinator|relaxer_lyrics_edit|relaxer_lyrics_persistence|relaxer_lyrics_save_work|relaxer_lyrics_replacement_policy|relaxer_qml_error_feedback|relaxer_playback_facade|relaxer_import_coordinator|relaxer_lyrics_replacement_integration|relaxer_qml_lyrics_workspace_parity|relaxer_qml_search_bridge|relaxer_translation_coordinator)$'
& "$QtPrefix/bin/ctest.exe" --test-dir $BuildDirectory -j 1 --output-on-failure -R $selection --output-log (Join-Path $EvidenceDirectory 'ctest.log')
if ($LASTEXITCODE) { throw 'Targeted tests failed; retain the original output.' }
if ($NativeSmoke) {
    $env:QT_QPA_PLATFORM = 'windows'
    foreach ($mode in @('qml','widgets')) {
        $smokeData = Join-Path $testTemp ("smoke-$mode-" + [guid]::NewGuid().ToString('N'))
        $smokeDiagnostics = Join-Path $EvidenceDirectory "smoke-$mode.json"
        $smokeProcess = Start-Process -FilePath (Join-Path $BuildDirectory 'Relaxer.exe') -ArgumentList @(
            "--ui=$mode",'--data',('"{0}"' -f $smokeData),
            '--diagnostics',('"{0}"' -f $smokeDiagnostics),'--quit-after=600'
        ) -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput (Join-Path $EvidenceDirectory "smoke-$mode.log") -RedirectStandardError (Join-Path $EvidenceDirectory "smoke-$mode-stderr.log")
        $smokeResult = Get-Content -LiteralPath $smokeDiagnostics -Raw | ConvertFrom-Json
        if ($smokeProcess.ExitCode -ne 0 -or $smokeResult.exitCode -ne 0 -or !$smokeResult.saveSucceeded) { throw "$mode startup/exit failed" }
        Write-Output "$mode native automatic startup/exit: process=$($smokeProcess.ExitCode), saveSucceeded=$($smokeResult.saveSucceeded)"
    }
}
