$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
. (Join-Path $PSScriptRoot 'owned-process.ps1')
$source = $env:RELAXER_SOURCE_ROOT
$stage = Join-Path $source 'runtime.pending'
$final = Join-Path $source 'runtime'
function Read-VerificationLog([string]$Path) {
    if (Test-Path -LiteralPath $Path) { return ((Get-Content -LiteralPath $Path -Tail 20 -ErrorAction SilentlyContinue) -join "`n") }
    return ''
}
function Run-Checked([string]$File,[string[]]$Arguments,[string]$Tag,[int]$Timeout=90000) {
    $out = Join-Path $stage ($Tag + '.log')
    $err = Join-Path $stage ($Tag + '-error.log')
    $p = $null
    $started=[DateTime]::UtcNow
    $record=[ordered]@{task=$Tag;root=$stage;executable=$File;arguments=$Arguments;started=$started.ToString('o');status='ERROR';stdout=$out;stderr=$err;cleanup='not-needed'}
    try {
        $p = New-Object RelaxerVerification.NativeLease($File,$Arguments,$stage,$out,$err,$Tag)
        $record.pid=$p.Pid; $record.parentPid=$p.ParentPid; $record.createdFileTime=$p.Created
        $record.image=$p.Image; $record.commandLine=$p.Command; $record.ownership='CreateProcessW original retained handle; root only'
        if (-not $p.Wait($Timeout)) {
            $record.cleanup=[RelaxerVerification.Cleanup]::Run($p,1000,5000)
            $record.timedOut=$true; $record.exitCode=$p.ExitCode
            throw ('Timeout: ' + $Tag + "`n" + (Read-VerificationLog $out) + "`n" + (Read-VerificationLog $err))
        }
        $record.exitCode=$p.ExitCode
        if ($record.exitCode -ne 0) { throw ($Tag + ' failed: ' + $record.exitCode + "`n" + (Read-VerificationLog $out) + "`n" + (Read-VerificationLog $err)) }
        $record.status='PASS'
        if ($Tag -eq 'verify-audio-decode') { Write-Output (Read-VerificationLog $out) }
    } catch { $record.error=$_.Exception.Message; throw
    } finally {
        try {
            if ($null -ne $p) {
                try {
                    if (-not $p.Exited()) { $record.cleanup=[RelaxerVerification.Cleanup]::Run($p,1000,5000) }
                    $record.exitCode=$p.ExitCode
                } finally { $p.Dispose() }
            }
        } catch {
            $record.status='ERROR';$record.cleanupError=$_.Exception.Message;throw
        } finally {
            $record.finished=[DateTime]::UtcNow.ToString('o')
            [IO.File]::WriteAllText((Join-Path $stage ($Tag+'.result.json')),($record|ConvertTo-Json -Depth 6),(New-Object Text.UTF8Encoding($false)))
        }
    }
}
try {
    if (-not $source) { throw 'Project path missing' }
    if (Test-Path -LiteralPath $final) { throw 'Existing runtime preserved; extract to a fresh folder.' }
    if ((Get-Item -LiteralPath $stage -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Staging directory cannot be a junction.' }
    $marker=Join-Path $stage '.relaxer-staging-owner'
    if ((Get-Item -LiteralPath $marker -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Staging marker cannot be a link.' }
    if ([IO.File]::ReadAllText($marker).TrimEnd([char[]]@("`r","`n")) -cne $source) { throw 'Staging ownership mismatch.' }
    foreach ($relative in @('Relaxer.exe','platforms\qwindows.dll','sqldrivers\qsqlite.dll','multimedia\ffmpegmediaplugin.dll','bin\ffmpeg.exe','bin\ffprobe.exe','resources\icon.ico')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage $relative))) { throw ('Missing: '+$relative) }
    }
    $env:PATH="$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
    $env:RELAXER_DATA=Join-Path $stage 'verification-data'
    $env:QT_MEDIA_BACKEND='ffmpeg'
    foreach ($name in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_PLATFORM','QML_IMPORT_PATH','QML2_IMPORT_PATH','RELAXER_MSYS2_DIR','PYTHONHOME','PYTHONPATH')) { [Environment]::SetEnvironmentVariable($name,$null,'Process') }
    Run-Checked (Join-Path $stage 'bin\ffmpeg.exe') @('-version') 'verify-ffmpeg'
    Run-Checked (Join-Path $stage 'bin\ffprobe.exe') @('-version') 'verify-ffprobe'
    Run-Checked (Join-Path $stage 'Relaxer.exe') @('--self-test') 'verify-core'
    $image=Join-Path $stage 'verify-window.png'
    Run-Checked (Join-Path $stage 'Relaxer.exe') @('--screenshot',$image) 'verify-window'
    if (-not (Test-Path -LiteralPath $image) -or (Get-Item -LiteralPath $image).Length -lt 1024) { throw 'Qt window verification failed.' }
    if (Test-Path -LiteralPath (Join-Path $stage 'Qt6Quick.dll')) {
        $qmlImage=Join-Path $stage 'verify-qml-window.png'
        Run-Checked (Join-Path $stage 'Relaxer.exe') @('--ui=qml','--screenshot',$qmlImage) 'verify-qml-window'
        if (-not (Test-Path -LiteralPath $qmlImage) -or (Get-Item -LiteralPath $qmlImage).Length -lt 1024) { throw 'QML window verification failed.' }
    }
    # Decode real PCM through the bundled Qt multimedia plugin. A sound card is
    # deliberately not required: device enumeration/rendering is machine-specific.
    # --audio-device-smoke remains available as a separate hardware diagnostic.
    $fixture=Join-Path $stage 'verify-audio.wav'
    Run-Checked (Join-Path $stage 'bin\ffmpeg.exe') @('-v','error','-f','lavfi','-i','sine=frequency=440:duration=3','-y',$fixture) 'make-audio'
    Run-Checked (Join-Path $stage 'Relaxer.exe') @('--media-smoke',$fixture) 'verify-audio-decode' 30000
    Remove-Item -LiteralPath $fixture -Force
    $verificationData=[IO.Path]::GetFullPath((Join-Path $stage 'verification-data'))
    if (-not [RelaxerVerification.NativeLease]::Inside($stage,$verificationData)) { throw 'Verification data escaped staging root' }
    Remove-Item -LiteralPath $verificationData -Recurse -Force
    $productVersion=([Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $stage 'Relaxer.exe'))).ProductVersion
    if (-not $productVersion) { throw 'Verified runtime lacks its product version resource' }
    $proof=[ordered]@{version='1.5.0';readySchemaVersion='1.5.0';productVersion=$productVersion;verified_at=[DateTime]::UtcNow.ToString('o');windows_only_path=$true;audio_decode=$true;audio_device_required=$false;media_clock=$false}
    [IO.File]::WriteAllText((Join-Path $stage 'relaxer-ready.json'),($proof|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
    [IO.Directory]::Move($stage,$final)
    Remove-Item -LiteralPath (Join-Path $final '.relaxer-staging-owner') -Force
    Write-Output 'Standalone import, database, GUI and Qt audio decode checks passed (no audio output device required).'
    exit 0
} catch { Write-Output ('Verification failed; cache retained: '+$_.Exception.Message);exit 1 }
