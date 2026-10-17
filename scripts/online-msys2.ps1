# Windows PowerShell 5.1; no Python, Qt, installed 7-Zip or elevated rights needed.
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$project = $env:RELAXER_SOURCE_ROOT
$cache = $env:RELAXER_BUILD_CACHE
try {
    if (-not $project -or -not $cache) { throw 'The Relaxer build-cache paths were not supplied.' }
    $marker = Join-Path $cache '.relaxer-owned-cache'
    $expected = "Relaxer-owned-cache-v1`n" + $project + "`n"
    if (-not (Test-Path -LiteralPath $marker) -or [IO.File]::ReadAllText($marker) -cne $expected) {
        throw 'The build cache is not owned by this Relaxer project.'
    }
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'msys2-download.json') -Raw | ConvertFrom-Json
    $archive = Join-Path $cache $manifest.file
    $msys = Join-Path $cache 'msys64'
    $initialized = Join-Path $msys '.relaxer-initialized'
    if ((Test-Path -LiteralPath $initialized) -and [IO.File]::ReadAllText($initialized) -ceq $manifest.sha256) {
        if (-not (Test-Path -LiteralPath (Join-Path $msys 'usr\bin\bash.exe'))) { throw 'The cached MSYS2 installation is incomplete.' }
        Write-Output 'RELAXER_PHASE:reuse-environment'
        Write-Output 'The initialized Relaxer MSYS2 cache is ready; no download is needed.'
        exit 0
    }
    $valid = (Test-Path -LiteralPath $archive) -and (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -eq $manifest.sha256
    if (-not $valid) {
        $downloaded = $false
        foreach ($url in $manifest.urls) {
            $partial = $archive + '.partial'
            $response = $null; $stream = $null; $file = $null
            try {
                Write-Output ('Downloading the pinned official MSYS2 base archive: ' + $url)
                $request = [Net.HttpWebRequest]::Create([string]$url)
                $request.UserAgent = 'Relaxer/0.2.0-rc2'
                $request.Timeout = 60000; $request.ReadWriteTimeout = 60000
                $response = $request.GetResponse()
                $total = [long]$response.ContentLength
                if ($total -gt 209715200) { throw 'The archive exceeds the expected size limit.' }
                $stream = $response.GetResponseStream()
                $file = [IO.File]::Open($partial, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::None)
                $buffer = New-Object byte[] 262144
                [long]$done = 0; [long]$last = -1048576
                while (($count = $stream.Read($buffer, 0, $buffer.Length)) -gt 0) {
                    $file.Write($buffer, 0, $count); $done += $count
                    if ($done -gt 209715200) { throw 'The archive exceeds the expected size limit.' }
                    if ($done - $last -ge 1048576 -or ($total -gt 0 -and $done -eq $total)) {
                        Write-Output ("RELAXER_DOWNLOAD:" + $done + '/' + $total); $last = $done
                    }
                }
                $file.Dispose(); $file = $null
                $stream.Dispose(); $stream = $null
                $response.Dispose(); $response = $null
                if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash.ToLowerInvariant() -ne $manifest.sha256) {
                    throw 'MSYS2 SHA-256 verification failed; the downloaded file will not be executed.'
                }
                Move-Item -LiteralPath $partial -Destination $archive -Force
                $downloaded = $true; break
            } catch {
                Write-Output ('Download attempt failed: ' + $_.Exception.Message)
            } finally {
                if ($file) { $file.Dispose() }; if ($stream) { $stream.Dispose() }; if ($response) { $response.Dispose() }
            }
        }
        if (-not $downloaded) { throw 'Both official download locations failed. Check network/proxy settings and click Retry.' }
    }
    # A retry never mixes a partially extracted tree with a fresh extraction.
    # Preserve it for diagnostics; all of these paths are inside the owned cache.
    if (Test-Path -LiteralPath $msys) {
        Move-Item -LiteralPath $msys -Destination ($msys + '.partial-' + [Guid]::NewGuid().ToString('N'))
    }
    Write-Output 'SHA-256 verified. Extracting the official self-extracting archive in the owned cache.'
    Write-Output 'RELAXER_PHASE:extract'
    $extract = Start-Process -FilePath $archive -ArgumentList @('-y',('-o' + $cache)) -NoNewWindow -PassThru
    $null = $extract.Handle
    if (-not $extract.WaitForExit(600000)) { $extract.Kill(); throw 'MSYS2 extraction timed out.' }
    $extract.WaitForExit()
    $extractCode = $extract.ExitCode; $extract.Dispose()
    if ($extractCode -ne 0) { throw ('MSYS2 extraction failed, exit ' + $extractCode) }
    $bash = Join-Path $msys 'usr\bin\bash.exe'
    if (-not (Test-Path -LiteralPath $bash)) { throw 'The archive did not produce usr\bin\bash.exe.' }
    $env:MSYSTEM = 'UCRT64'; $env:CHERE_INVOKING = '1'; $env:MSYS2_PATH_TYPE = 'minimal'
    Write-Output 'Initializing the MSYS2 login shell.'
    Write-Output 'RELAXER_PHASE:initialize'
    & $bash '--login' '-c' 'exit 0'
    if ($LASTEXITCODE -ne 0) { throw ('MSYS2 initialization failed, exit ' + $LASTEXITCODE) }
    [IO.Directory]::CreateDirectory((Join-Path $msys 'ucrt64')) | Out-Null
    [IO.File]::WriteAllText($initialized, $manifest.sha256, (New-Object Text.UTF8Encoding($false)))
    Write-Output 'Independent MSYS2 base is initialized. Dependency installation is next.'
    exit 0
} catch {
    Write-Output ('Online preparation failed: ' + $_.Exception.Message)
    exit 1
}
