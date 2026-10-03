# LayerWand no-link baseline logger (Slice 1, Increment 2A).
#
# Reads LayerWand's USB serial port for the whole baseline run and writes
# every line, dumps included, to
#   <repo>\Claude outputs\baseline\layerwand_<timestamp>.log
# The console shows only progress: boot lines, phase changes, events, dump
# markers, a countdown, and a clear prompt for each BOOT press.
#
# Opening the port can reset LayerWand (observed 2026-09-30: the Windows
# serial open produced an ESP_RST_USB reset even with DTR and RTS requested
# low, so no claim is made that the open is reset-free). The protocol
# therefore accepts the first boot the logger sees running early warning as
# the BASELINE BOOT, whatever its reset reason, and requires that boot count
# to stay unchanged through both runs. A reset during a run is flagged, the
# run is void, and the logger starts over on the new boot without stopping,
# so the reset evidence and the retry are in the same file.
#
# Run (from any PowerShell window; leave the window open for the whole run):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_baseline_log.ps1"
# Stop with Ctrl+C when the script says both runs are dumped.
#
# Sequence: plug LayerWand in, start this script, do nothing else. Run A
# (early warning) starts at the baseline boot; the script says when to press
# BOOT. Run B (manual ALL) starts on that press; it says when to press BOOT
# again. Then stop.

param(
    [int]$RunMinutes = 60,
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime',
    # Self-test only: replay a saved log through the progress logic instead of
    # reading a serial port. Not used for a real baseline.
    [string]$ReplayFile = ''
)

$ErrorActionPreference = 'Stop'

$logDir = Join-Path $RepoDir 'Claude outputs\baseline'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$logPath = Join-Path $logDir ('layerwand_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.log')
$writer = New-Object System.IO.StreamWriter($logPath, $true, [System.Text.Encoding]::UTF8)
$writer.AutoFlush = $true

function Now { Get-Date -Format 'HH:mm:ss' }
function Say([string]$text) { Write-Host ((Now) + '  ' + $text) }
function Shout([string]$text) { Write-Host ((Now) + '  ' + $text) -ForegroundColor Yellow -BackgroundColor DarkBlue }
function Alarm([string]$text) { Write-Host ((Now) + '  ' + $text) -ForegroundColor White -BackgroundColor DarkRed }
function Note([string]$text) { $writer.WriteLine('# ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ' + $text) }

function Find-LayerWandPort {
    # Espressif USB Serial/JTAG: vendor 303A, product 1001.
    foreach ($d in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'USB\VID_303A&PID_1001*' })) {
        if ($d.Name -match '\((COM\d+)\)') { return $Matches[1] }
    }
    return $null
}

Say "Logging LayerWand to $logPath"
Say "Waiting for LayerWand's serial port. Plug LayerWand in if it is not; do nothing else."
Say 'Opening the port may reset LayerWand; the first boot seen running early warning becomes the baseline boot.'

$port = $null
$lastOpenError = ''
$replay = $null
if ($ReplayFile -ne '') { $replay = [System.IO.File]::ReadLines($ReplayFile).GetEnumerator(); Say "Replaying $ReplayFile (self-test)." }
$bootCount = $null          # boot count of the baseline boot, from the reset lines
$runName = ''               # 'A' or 'B'
$runStart = $null           # DateTime the current run started (already offset by the device's run time)
$nextReminder = $null
$stage = 'waiting-for-boot' # waiting-for-boot, run-a, run-b, finished

function Start-RunA([int]$alreadyRunS) {
    $script:stage = 'run-a'; $script:runName = 'A'
    $script:runStart = (Get-Date).AddSeconds(-$alreadyRunS); $script:nextReminder = $null
    Shout ("RUN A (early warning) STARTED (device run time $alreadyRunS s). Leave LayerWand alone; BOOT press due at " + $script:runStart.AddMinutes($RunMinutes).ToString('HH:mm:ss') + '.')
}

try {
    while ($true) {
        if ($null -ne $replay) {
            if (-not $replay.MoveNext()) { Say 'Replay finished.'; break }
            $line = $replay.Current
            Start-Sleep -Milliseconds 20
        } elseif ($null -eq $port) {
            $name = Find-LayerWandPort
            if ($null -eq $name) { Start-Sleep -Seconds 1; continue }
            $candidate = New-Object System.IO.Ports.SerialPort($name, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
            $candidate.DtrEnable = $false
            $candidate.RtsEnable = $false
            $candidate.NewLine = "`n"
            $candidate.ReadTimeout = 2000
            try {
                $candidate.Open()
            } catch {
                if ($_.Exception.Message -ne $lastOpenError) {
                    Alarm "Cannot open $name : $($_.Exception.Message)"
                    Say 'If the pioarduino serial monitor or another logger holds the port, close it. Retrying every second.'
                    $lastOpenError = $_.Exception.Message
                }
                Start-Sleep -Seconds 1
                continue
            }
            $port = $candidate
            $lastOpenError = ''
            Note "opened $name"
            Say "Opened $name."
        }

        # Countdown and BOOT prompts, independent of serial traffic.
        if ($null -ne $runStart) {
            $elapsed = (Get-Date) - $runStart
            $remaining = [TimeSpan]::FromMinutes($RunMinutes) - $elapsed
            if ($remaining.TotalSeconds -le 0) {
                if ($stage -eq 'run-a') { Shout "RUN A COMPLETE ($RunMinutes min). PRESS BOOT ONCE NOW (short press). Run B (manual ALL) starts on the press." }
                elseif ($stage -eq 'run-b') { Shout "RUN B COMPLETE ($RunMinutes min). PRESS BOOT ONCE NOW (short press). The dump follows." }
                $nextReminder = (Get-Date).AddSeconds(30)
                $runStart = $null
            } elseif ($null -eq $nextReminder -or (Get-Date) -ge $nextReminder) {
                Say ("Run $runName in progress: {0} min remaining. Do not touch LayerWand." -f [int][Math]::Ceiling($remaining.TotalMinutes))
                $nextReminder = (Get-Date).AddMinutes(5)
            }
        } elseif ($null -ne $nextReminder -and (Get-Date) -ge $nextReminder -and ($stage -eq 'run-a' -or $stage -eq 'run-b')) {
            Shout 'Still waiting for the BOOT press.'
            $nextReminder = (Get-Date).AddSeconds(30)
        }

        if ($null -ne $replay) {
        } else {
        try {
            $line = $port.ReadLine().TrimEnd("`r")
        } catch [System.TimeoutException] {
            continue
        } catch {
            Note 'port lost'
            Alarm 'Port lost (replug or reset). Waiting for LayerWand.'
            try { $port.Close() } catch {}
            $port = $null
            Start-Sleep -Seconds 1
            continue
        }
        }

        $writer.WriteLine($line)

        # Boot record: "[periodic] reset <name> (<code>), boot N, previous boot: ..."
        if ($line -match '^\[(periodic|boot|dump)\] reset (.+?) \((\d+)\), boot (\d+),') {
            $reason = $Matches[2]
            $n = [int]$Matches[4]
            if ($null -eq $bootCount) {
                $bootCount = $n
                Note "baseline boot $n reason $reason"
                Say "Baseline boot $n, reset reason: $reason. This boot count must not change until both runs are dumped."
            } elseif ($n -ne $bootCount) {
                Note "boot count changed $bootCount -> $n reason $reason"
                if ($stage -eq 'run-a' -or $stage -eq 'run-b' -or $stage -eq 'finished') {
                    Alarm "LAYERWAND RESET: boot $bootCount -> $n (reason: $reason). THE RUN IN PROGRESS IS VOID. Starting over with boot $n as the baseline boot; keep logging."
                } else {
                    Say "New boot $n, reset reason: $reason; it is now the baseline boot."
                }
                $bootCount = $n
                $stage = 'waiting-for-boot'
                $runStart = $null
                $runName = ''
                $nextReminder = $null
            }
        }

        # Run starts. "=== run start mode <mode> at <uptime> ms". An early-warning
        # start within the first 30 s of uptime is the boot start (Run A), on any
        # boot; one after a BOOT press from Stopped is also a Run A.
        if ($line -match '^=== run start mode (\S+) at (\d+) ms') {
            $mode = $Matches[1]
            $uptimeMs = [long]$Matches[2]
            if ($mode -eq 'early-warning' -and ($stage -eq 'waiting-for-boot' -or ($uptimeMs -lt 30000 -and $stage -eq 'run-a'))) {
                if ($stage -eq 'run-a') { Alarm 'LayerWand restarted during Run A (early-warning start at boot). The previous attempt is void; timing restarts now.' }
                Start-RunA 0
            } elseif ($mode -eq 'manual-all' -and $stage -eq 'run-a') {
                $stage = 'run-b'; $runName = 'B'; $runStart = Get-Date; $nextReminder = $null
                Shout ("RUN B (manual ALL) STARTED. Leave LayerWand alone for $RunMinutes min; BOOT press due at " + $runStart.AddMinutes($RunMinutes).ToString('HH:mm:ss') + '.')
            } elseif ($mode -eq 'stopped' -and $stage -eq 'run-b') {
                $stage = 'finished'; $runStart = $null; $nextReminder = $null
                Shout 'BOTH RUNS DUMPED. Wait for one more [periodic] line (about 10 s), then press Ctrl+C to close the log.'
            } else {
                Say "Run start: $mode (stage $stage)."
            }
        }

        # If the boot's run-start line was lost (port not open yet), the first
        # periodic report of an early-warning run tells us how long it has run.
        if ($stage -eq 'waiting-for-boot' -and $line -match '^\[periodic\] mode early-warning run (\d+) s ') {
            Start-RunA ([int]$Matches[1])
        }

        if ($line -match '^(=== run dump (begin|end)|Button press|Link:|LayerWand|phase |event )') {
            Say $line.Substring(0, [Math]::Min(110, $line.Length))
        } elseif ($line -match '^\[periodic\] mode' -and $stage -eq 'finished') {
            Say 'Final periodic report captured. Press Ctrl+C now.'
        } elseif ($line -match 'CORRUPT|PSRAM allocation FAILED|NO RING') {
            Alarm $line
        }
    }
} finally {
    if ($port) { try { $port.Close() } catch {} }
    Note 'logger closed'
    $writer.Close()
    Write-Host ''
    Write-Host "Log closed: $logPath"
}
