param(
    [ValidateSet(0, 50, 100, 150, 200)][int]$LatencyMs = 0,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [ValidateRange(1024, 65535)][int]$Port = 18777,
    [switch]$SmokeTest,
    [switch]$Benchmark,
    [switch]$RewindBenchmark,
    [switch]$LiveExperimentCheck,
    [switch]$CombatFeedbackCheck,
    [switch]$MovingAimCheck,
    [switch]$FireModeCheck,
    [switch]$DisableLagCompensation,
    [ValidateRange(8, 60)][int]$Samples = 24,
    [switch]$CaptureFrames,
    [ValidateRange(640, 3840)][int]$WindowWidth = 960,
    [ValidateRange(360, 2160)][int]$WindowHeight = 540,
    [switch]$ShowWindows
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Latency_MultiCombat.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
if (!(Test-Path -LiteralPath $editor)) { throw "UE executable missing: $editor" }
if (!(Test-Path -LiteralPath (Join-Path $projectRoot 'Content\Level\FPSBaseline.umap'))) { throw 'Generate FPSBaseline.umap first.' }
if ($SmokeTest -and ($Benchmark -or $RewindBenchmark -or $LiveExperimentCheck -or $ShowWindows -or $CaptureFrames)) { throw 'SmokeTest cannot be combined with a benchmark, ShowWindows or CaptureFrames.' }
if ($Benchmark -and $RewindBenchmark) { throw 'Select one benchmark mode.' }
if ($LiveExperimentCheck -and ($Benchmark -or $RewindBenchmark)) { throw 'LiveExperimentCheck must run by itself.' }
if ($CombatFeedbackCheck -and !$Benchmark) { throw 'CombatFeedbackCheck requires Benchmark.' }
if ($MovingAimCheck -and !$Benchmark) { throw 'MovingAimCheck requires Benchmark.' }
if ($FireModeCheck -and !$Benchmark) { throw 'FireModeCheck requires Benchmark.' }
if ($RewindBenchmark -and $Samples -gt 24) { throw 'RewindBenchmark supports 8 through 24 samples.' }
if ($CaptureFrames -and !$Benchmark -and !$RewindBenchmark -and !$LiveExperimentCheck) { throw 'CaptureFrames requires a benchmark.' }
$mode = if ($LiveExperimentCheck) { 'live-experiment' } elseif ($RewindBenchmark) { 'rewind' } elseif ($Benchmark) { 'benchmark' } elseif ($SmokeTest) { 'smoke' } else { 'play' }
$runName = '{0}-{1}ms-{2}' -f (Get-Date -Format 'yyyyMMdd-HHmmss-fff'), $LatencyMs, $mode
$logDirectory = Join-Path $projectRoot "Saved\FPSBaseline\$runName"
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
$serverLog = Join-Path $logDirectory 'Server.log'
$shooterLog = Join-Path $logDirectory 'Shooter.log'
$observerLog = Join-Path $logDirectory 'Observer.log'
$processes = @()
try {
    # Each sender delays outgoing packets: expected added RTT is approximately 2 * LatencyMs.
    $baseArgs = @(('"' + $projectFile + '"'), '-unattended', '-nosplash', '-NoSound', "-PktLag=$LatencyMs", '-PktLagVariance=0', '-PktLoss=0', '-ExecCmds="t.MaxFPS 60"')
    $serverArgs = $baseArgs + @('/Game/Level/FPSBaseline', '-server', '-nullrhi', "-port=$Port", ('-abslog="' + $serverLog + '"'))
    if ($DisableLagCompensation) { $serverArgs += '-NoLagCompensation' }
    if ($RewindBenchmark) { $serverArgs += @('-FPSRewindBenchmark', "-FPSBenchmarkSamples=$Samples") }
    $server = Start-Process -FilePath $editor -ArgumentList $serverArgs -WindowStyle Hidden -PassThru
    $processes += $server
    $deadline = (Get-Date).AddSeconds(60)
    while (!(Test-Path -LiteralPath $serverLog) -or !(Select-String -LiteralPath $serverLog -Pattern 'IpNetDriver listening on port' -Quiet)) {
        if ($server.HasExited) { throw "Server exited. Inspect $serverLog" }
        if ((Get-Date) -gt $deadline) { throw "Server startup timed out. Inspect $serverLog" }
        Start-Sleep -Milliseconds 250
    }
    $clientArgs = $baseArgs + @("127.0.0.1:$Port", '-game')
    $windowStyle = if ($ShowWindows) { 'Normal' } else { 'Hidden' }
    if ($SmokeTest -or (($Benchmark -or $RewindBenchmark -or $LiveExperimentCheck) -and !$ShowWindows -and !$CaptureFrames)) { $clientArgs += '-nullrhi' }
    else { $clientArgs += @('-windowed', "-ResX=$WindowWidth", "-ResY=$WindowHeight") }
    if ($CaptureFrames -and !$ShowWindows) { $clientArgs += '-RenderOffscreen' }
    if ($SmokeTest) { $clientArgs += '-FPSBaselineSmoke' }
    if ($Benchmark) { $clientArgs += @('-FPSBaselineBenchmark', "-FPSBenchmarkSamples=$Samples") }
    if ($RewindBenchmark) { $clientArgs += @('-FPSRewindBenchmark', "-FPSBenchmarkSamples=$Samples") }
    if ($LiveExperimentCheck) { $clientArgs += '-FPSLiveExperimentCheck' }
    if ($CombatFeedbackCheck) { $clientArgs += '-FPSCombatFeedbackCheck' }
    if ($MovingAimCheck) { $clientArgs += '-FPSMovingAimCheck' }
    if ($FireModeCheck) { $clientArgs += '-FPSFireModeCheck' }
    if ($CaptureFrames) { $clientArgs += @('-FPSBenchmarkCapture', '-ForceRes') }
    $shooterArgs = $clientArgs + @(('-abslog="' + $shooterLog + '"'))
    if ($SmokeTest -or $Benchmark -or $RewindBenchmark -or $LiveExperimentCheck) { $shooterArgs += '-FPSBaselineShooter' }
    $shooter = Start-Process -FilePath $editor -ArgumentList $shooterArgs -WindowStyle $windowStyle -PassThru
    $processes += $shooter
    # First connection receives Red; start Blue after the server accepts Red.
    $deadline = (Get-Date).AddSeconds(60)
    while (!(Select-String -LiteralPath $serverLog -Pattern 'Join succeeded' -Quiet)) {
        if ($shooter.HasExited) { throw "First client exited. Inspect $shooterLog" }
        if ((Get-Date) -gt $deadline) { throw 'First client connection timed out.' }
        Start-Sleep -Milliseconds 250
    }
    $observer = Start-Process -FilePath $editor -ArgumentList ($clientArgs + @(('-abslog="' + $observerLog + '"'))) -WindowStyle $windowStyle -PassThru
    $processes += $observer
    Write-Host "[$mode $LatencyMs ms] Server $($server.Id), clients $($shooter.Id), $($observer.Id). Logs: $logDirectory"
    if (!$SmokeTest -and !$Benchmark -and !$RewindBenchmark -and !$LiveExperimentCheck) {
        Write-Host 'Close both clients when finished; this script will then stop its server.'
        Wait-Process -Id $shooter.Id, $observer.Id
        return
    }
    $deadline = (Get-Date).AddSeconds($(if ($Benchmark -or $RewindBenchmark -or $LiveExperimentCheck) { 180 } else { 75 }))
    while (!$shooter.HasExited -or !$observer.HasExited) {
        if ((Get-Date) -gt $deadline) { throw "$mode timed out. Inspect $logDirectory" }
        Start-Sleep -Milliseconds 250
    }
    $tag = if ($RewindBenchmark) { 'FPSRewindBenchmark' } elseif ($Benchmark -or $LiveExperimentCheck) { 'FPSBaselineBenchmark' } else { 'FPSBaselineSmoke' }
    foreach ($log in @($shooterLog, $observerLog)) {
        if (!(Select-String -LiteralPath $log -Pattern "\[$tag\] PASS" -Quiet)) { throw "$mode did not pass: $log" }
        if (Select-String -LiteralPath $log -Pattern "\[$tag\] FAIL|Fatal error:|Assertion failed:" -Quiet) { throw "$mode failed: $log" }
    }
    [pscustomobject]@{ LatencyMs = $LatencyMs; AddedRttMs = 2 * $LatencyMs; LagCompensation = !$DisableLagCompensation; Result = 'PASS'; Logs = $logDirectory }
}
finally {
    # Terminate only processes launched by this invocation.
    foreach ($process in $processes) {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    }
}
