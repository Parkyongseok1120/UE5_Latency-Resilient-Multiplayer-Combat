param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [int]$Port = 18777,
    [ValidateRange(1, 10)][int]$Repeats = 1,
    [ValidateRange(8, 24)][int]$Samples = 24,
    [ValidateSet(0, 50, 100, 150, 200)][int[]]$Latencies = @(0, 50, 100, 150, 200),
    [ValidateSet('On', 'Off')][string[]]$Modes = @('Off', 'On'),
    [switch]$CaptureFrames
)
$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$projectRoot = Split-Path $PSScriptRoot -Parent
$outputDirectory = Join-Path $projectRoot ('Saved\FPSBaseline\RewindMeasurements\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$rows = [Collections.Generic.List[object]]::new()
$runs = [Collections.Generic.List[object]]::new()
$pattern = '\[FPSRewindBenchmark\] SAMPLE Shot=(\d+) Rewind=(\d) Expected=(\d) Actual=(\d) Rejection=(\d+) AckMs=([\d.]+) RttMs=([\d.]+) ShotAgeMs=([\d.]+) TargetAgeMs=([\d.]+) TargetY=(-?[\d.]+) SpeedCmS=([\d.]+)'
foreach ($latency in $Latencies) {
    foreach ($mode in $Modes) {
        for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
            Write-Host "Moving target: lag=$latency ms rewind=$mode repetition=$repeat/$Repeats"
            $run = & (Join-Path $PSScriptRoot 'Start-FPSBaseline.ps1') -LatencyMs $latency -EngineRoot $EngineRoot -Port $Port -RewindBenchmark -Samples $Samples -DisableLagCompensation:($mode -eq 'Off') -CaptureFrames:$CaptureFrames
            $count = 0
            foreach ($line in Get-Content -LiteralPath (Join-Path $run.Logs 'Shooter.log')) {
                if ($line -match $pattern) {
                    $rows.Add([pscustomobject]@{
                        LatencyMs = $latency; AddedRttMs = 2 * $latency; Mode = $mode; Repetition = $repeat
                        ShotId = [int]$Matches[1]; UsedRewind = [int]$Matches[2]; Expected = [int]$Matches[3]
                        Actual = [int]$Matches[4]; Rejection = [int]$Matches[5]
                        AckMs = [double]::Parse($Matches[6], $culture); RttMs = [double]::Parse($Matches[7], $culture)
                        ShotAgeMs = [double]::Parse($Matches[8], $culture); TargetAgeMs = [double]::Parse($Matches[9], $culture)
                        TargetY = [double]::Parse($Matches[10], $culture); SpeedCmS = [double]::Parse($Matches[11], $culture)
                        Logs = $run.Logs
                    })
                    $count++
                }
            }
            if ($count -ne $Samples) { throw "Expected $Samples samples, received $count. $($run.Logs)" }
            $runs.Add([pscustomobject]@{ LatencyMs = $latency; Mode = $mode; Repetition = $repeat; Samples = $count; Result = $run.Result; Logs = $run.Logs })
            $rows | Export-Csv -LiteralPath (Join-Path $outputDirectory 'RawSamples.csv') -NoTypeInformation -Encoding UTF8
            $runs | Export-Csv -LiteralPath (Join-Path $outputDirectory 'Runs.csv') -NoTypeInformation -Encoding UTF8
        }
    }
}
$summary = foreach ($group in $rows | Group-Object LatencyMs, Mode) {
    $data = @($group.Group)
    $ack = @($data.AckMs | Sort-Object)
    $tp = @($data | Where-Object { $_.Expected -eq 1 -and $_.Actual -eq 1 }).Count
    $tn = @($data | Where-Object { $_.Expected -eq 0 -and $_.Actual -eq 0 }).Count
    $fp = @($data | Where-Object { $_.Expected -eq 0 -and $_.Actual -eq 1 }).Count
    $fn = @($data | Where-Object { $_.Expected -eq 1 -and $_.Actual -eq 0 }).Count
    [pscustomobject]@{
        LatencyMs = $data[0].LatencyMs; Mode = $data[0].Mode; Samples = $data.Count
        TruePositive = $tp; TrueNegative = $tn; FalsePositive = $fp; FalseNegative = $fn
        RecallPercent = $(if ($tp + $fn) { [Math]::Round(100.0 * $tp / ($tp + $fn), 2) } else { $null })
        AgreementPercent = [Math]::Round(100.0 * ($tp + $tn) / $data.Count, 2)
        Rejected = @($data | Where-Object Rejection -ne 0).Count
        AckP50Ms = $ack[[Math]::Ceiling($ack.Count * 0.50) - 1]
        AckP95Ms = $ack[[Math]::Ceiling($ack.Count * 0.95) - 1]
        MeanRttMs = [Math]::Round(($data.RttMs | Measure-Object -Average).Average, 3)
    }
}
$summary | Export-Csv -LiteralPath (Join-Path $outputDirectory 'Summary.csv') -NoTypeInformation -Encoding UTF8
$summary | Format-Table -AutoSize | Out-Host
[pscustomobject]@{ OutputDirectory = $outputDirectory; Runs = $runs.Count; Samples = $rows.Count }
