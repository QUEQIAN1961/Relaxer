$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
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
    try {
        $p = Start-Process -FilePath $File -ArgumentList $Arguments -WorkingDirectory $stage -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError $err
        $null=$p.Handle
        if (-not $p.WaitForExit($Timeout)) {
            # Stop only the verification process we started and its worker.
            $taskkill = Join-Path $env:SystemRoot 'System32\taskkill.exe'
            if (Test-Path -LiteralPath $taskkill) { & $taskkill /PID $p.Id /T /F 2>&1 | Out-Null }
            if (-not $p.HasExited) { $p.Kill() }
            $null=$p.WaitForExit(1000)
            throw ('Timeout: ' + $Tag + "`n" + (Read-VerificationLog $out) + "`n" + (Read-VerificationLog $err))
        }
        $p.WaitForExit()
        if ($p.ExitCode -ne 0) { throw ($Tag + ' failed: ' + $p.ExitCode + "`n" + (Read-VerificationLog $out) + "`n" + (Read-VerificationLog $err)) }
        if ($Tag -eq 'verify-audio-decode') { Write-Output (Read-VerificationLog $out) }
    } finally { if ($null -ne $p) { $p.Dispose() } }
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
    Run-Checked (Join-Path $stage 'Relaxer.exe') @('--screenshot',('"'+$image+'"')) 'verify-window'
    if (-not (Test-Path -LiteralPath $image) -or (Get-Item -LiteralPath $image).Length -lt 1024) { throw 'Qt window verification failed.' }
    # Decode real PCM through the bundled Qt multimedia plugin. A sound card is
    # deliberately not required: device enumeration/rendering is machine-specific.
    # --audio-device-smoke remains available as a separate hardware diagnostic.
    $fixture=Join-Path $stage 'verify-audio.wav'
    Run-Checked (Join-Path $stage 'bin\ffmpeg.exe') @('-v','error','-f','lavfi','-i','sine=frequency=440:duration=3','-y',('"'+$fixture+'"')) 'make-audio'
    Run-Checked (Join-Path $stage 'Relaxer.exe') @('--media-smoke',('"'+$fixture+'"')) 'verify-audio-decode' 30000
    Remove-Item -LiteralPath $fixture -Force
    Remove-Item -LiteralPath (Join-Path $stage 'verification-data') -Recurse -Force
    $proof=[ordered]@{version='1.5.0';verified_at=[DateTime]::UtcNow.ToString('o');windows_only_path=$true;audio_decode=$true;audio_device_required=$false;media_clock=$false}
    [IO.File]::WriteAllText((Join-Path $stage 'relaxer-ready.json'),($proof|ConvertTo-Json),(New-Object Text.UTF8Encoding($false)))
    [IO.Directory]::Move($stage,$final)
    Remove-Item -LiteralPath (Join-Path $final '.relaxer-staging-owner') -Force
    Write-Output 'Standalone import, database, GUI and Qt audio decode checks passed (no audio output device required).'
    exit 0
} catch { Write-Output ('Verification failed; cache retained: '+$_.Exception.Message);exit 1 }
