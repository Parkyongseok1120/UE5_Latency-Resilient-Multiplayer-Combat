param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [int]$Port = 18777,
    [ValidateRange(1, 10)][int]$Repeats = 3,
    [ValidateRange(8, 60)][int]$Samples = 24,
    [ValidateSet(0, 50, 100, 150, 200)][int[]]$Latencies = @(0, 50, 100, 150, 200)
)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$projectRoot = Split-Path $PSScriptRoot -Parent
$outputDirectory = Join-Path $projectRoot ('Saved\FPSBaseline\Measurements\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$rows = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$pattern = '\[FPSBaselineBenchmark\] SAMPLE Shot=(\d+) FeedbackMs=([\d.]+) HitConfirmMs=(-?[\d.]+) HealthMs=(-?[\d.]+) Hit=(\d) HealthBefore=([\d.]+) HealthAfter=([\d.]+) RangeCm=([\d.]+)'
foreach ($latency in $Latencies) {
    for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
        Write-Host "Measuring $latency ms, repetition $repeat/$Repeats ($Samples shots)"
        $run = & (Join-Path $PSScriptRoot 'Start-FPSBaseline.ps1') -LatencyMs $latency -EngineRoot $EngineRoot -Port $Port -Benchmark -Samples $Samples
        $count = 0
        foreach ($line in Get-Content -LiteralPath (Join-Path $run.Logs 'Shooter.log')) {
            if ($line -match $pattern) {
                $feedback = [double]::Parse($Matches[2], $culture)
                $confirm = [double]::Parse($Matches[3], $culture)
                $health = [double]::Parse($Matches[4], $culture)
                $rows.Add([pscustomobject]@{
                    LatencyMs = $latency; AddedRttMs = 2 * $latency; Repetition = $repeat; ShotId = [int]$Matches[1]
                    FeedbackMs = $feedback; HitConfirmMs = $(if ($confirm -ge 0) { $confirm } else { $null })
                    HealthMs = $(if ($health -ge 0) { $health } else { $null }); Hit = [int]$Matches[5]
                    HealthBefore = [double]::Parse($Matches[6], $culture); HealthAfter = [double]::Parse($Matches[7], $culture); Logs = $run.Logs
                    RangeCm = [double]::Parse($Matches[8], $culture)
                })
                $count++
            }
        }
        if ($count -ne $Samples) { throw "Expected $Samples samples, got ${count}: $($run.Logs)" }
        $runs.Add([pscustomobject]@{ LatencyMs = $latency; Repetition = $repeat; Samples = $count; Result = $run.Result; Logs = $run.Logs })
        $rows | Export-Csv -LiteralPath (Join-Path $outputDirectory 'RawSamples.csv') -NoTypeInformation -Encoding UTF8
        $runs | Export-Csv -LiteralPath (Join-Path $outputDirectory 'Runs.csv') -NoTypeInformation -Encoding UTF8
    }
}
function Get-MetricStats($values) {
    $sorted = @($values | Where-Object { $null -ne $_ } | Sort-Object)
    if (!$sorted.Count) { return [pscustomobject]@{ Mean = $null; P50 = $null; P95 = $null } }
    [pscustomobject]@{
        Mean = [Math]::Round(($sorted | Measure-Object -Average).Average, 3)
        P50 = $sorted[[Math]::Ceiling($sorted.Count * 0.50) - 1]
        P95 = $sorted[[Math]::Ceiling($sorted.Count * 0.95) - 1]
    }
}
$summary = foreach ($latency in $Latencies) {
    $group = @($rows | Where-Object LatencyMs -eq $latency)
    $hits = @($group | Where-Object Hit -eq 1)
    $feedback = Get-MetricStats @($group | ForEach-Object FeedbackMs)
    $confirm = Get-MetricStats @($hits | ForEach-Object HitConfirmMs)
    $health = Get-MetricStats @($hits | ForEach-Object HealthMs)
    $range = $group | ForEach-Object RangeCm | Measure-Object -Minimum -Maximum -Average
    [pscustomobject]@{
        LatencyMs = $latency; AddedRttMs = 2 * $latency; Repetitions = $Repeats; Samples = $group.Count; Hits = $hits.Count
        HitRatePercent = [Math]::Round(100.0 * $hits.Count / $group.Count, 2)
        FeedbackMeanMs = $feedback.Mean; FeedbackP50Ms = $feedback.P50; FeedbackP95Ms = $feedback.P95
        HitConfirmMeanMs = $confirm.Mean; HitConfirmP95Ms = $confirm.P95
        HealthMeanMs = $health.Mean; HealthP50Ms = $health.P50; HealthP95Ms = $health.P95
        RangeMeanCm = [Math]::Round($range.Average, 3); RangeMinCm = $range.Minimum; RangeMaxCm = $range.Maximum
    }
}
$summary | Export-Csv -LiteralPath (Join-Path $outputDirectory 'Summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Format-Table LatencyMs, Samples, Hits, HitRatePercent, FeedbackMeanMs, HealthMeanMs, HealthP95Ms -AutoSize | Out-Host
Write-Host "Measurement files: $outputDirectory"
[pscustomobject]@{ OutputDirectory = $outputDirectory; Runs = $runs.Count; Samples = $rows.Count }
