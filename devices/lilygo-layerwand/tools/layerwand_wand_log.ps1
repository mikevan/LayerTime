# LayerWand logger for Increment 2B (architecture A), for the SECOND
# LayerWand only.
#
# Reads the USB serial port of the one LayerWand whose USB serial number you
# name with -Serial, and writes every line to
#   <repo>\Claude outputs\wand2\layerwand_wand_<timestamp>.log
# It never opens any other port, so the Increment 2A baseline LayerWand and
# its logger are left alone. Run without -Serial to list the LayerWands that
# are plugged in (no port is opened).
#
# The console shows progress only: boots and reset reasons, link connects
# and disconnects, commands from the watch, Recon phase and run changes,
# detections, one line per periodic report, and the 2B validation checklist
# as each item is first seen in the log:
#   1 Recon running with no watch connected
#   2 watch connected
#   3 detections delivered to the watch (GET_CHANGED summaries sent)
#   4 a Control from the watch answered (command ... result 0)
#   5 watch disconnected, Recon still running
#   6 watch reconnected
# A LayerWand reset is flagged with its reason and the logger keeps going on
# the new boot, so the evidence stays in one file.
#
# Opening the port can reset LayerWand (observed 2026-09-30 on the baseline
# device: an ESP_RST_USB reset on the Windows serial open). Every boot line
# says why that boot happened, so an open-time reset is visible in the log.
#
# Run (leave the window open; stop with Ctrl+C):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_wand_log.ps1" -Serial <serial>

param(
    [string]$Serial = '',
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime'
)

$ErrorActionPreference = 'Stop'

function Now { Get-Date -Format 'HH:mm:ss' }
function Say([string]$text) { Write-Host ((Now) + '  ' + $text) }
function Shout([string]$text) { Write-Host ((Now) + '  ' + $text) -ForegroundColor Yellow -BackgroundColor DarkBlue }
function Alarm([string]$text) { Write-Host ((Now) + '  ' + $text) -ForegroundColor White -BackgroundColor DarkRed }

# Every Espressif USB Serial/JTAG port (VID 303A, PID 1001) with its COM
# port and USB serial number. Reads the Windows device tree only.
function Get-LayerWands {
    $found = @()
    foreach ($d in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'USB\VID_303A&PID_1001*' })) {
        if ($d.Name -notmatch '\((COM\d+)\)') { continue }
        $port = $Matches[1]
        $id = $d.PNPDeviceID
        if ($id -match '&MI_\d\d') {
            try {
                $parent = (Get-PnpDeviceProperty -InstanceId $id -KeyName 'DEVPKEY_Device_Parent' -ErrorAction Stop).Data
                if ($parent -like 'USB\VID_303A&PID_1001\*') { $id = $parent }
            } catch {
                # Without the parent the interface's own id is listed.
            }
        }
        $found += [pscustomobject]@{ Port = $port; Serial = ($id -split '\\')[-1] }
    }
    return $found
}

function Find-Port([string]$serial) {
    $m = @(Get-LayerWands | Where-Object { $_.Serial -eq $serial })
    if ($m.Count -eq 1) { return $m[0].Port }
    return $null
}

if ($Serial -eq '') {
    $wands = @(Get-LayerWands)
    if ($wands.Count -eq 0) { Write-Host 'No LayerWand is plugged in.'; exit 1 }
    $wands | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host 'Name the second LayerWand with -Serial <serial>. Do not name the baseline LayerWand.'
    exit 0
}

$logDir = Join-Path $RepoDir 'Claude outputs\wand2'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$logPath = Join-Path $logDir ('layerwand_wand_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '.log')
$writer = New-Object System.IO.StreamWriter($logPath, $true, [System.Text.Encoding]::UTF8)
$writer.AutoFlush = $true
function Note([string]$text) { $writer.WriteLine('# ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') + ' ' + $text) }

$checks = [ordered]@{
    '1' = 'Recon running with no watch connected'
    '2' = 'watch connected'
    '3' = 'detections delivered to the watch'
    '4' = 'a Control from the watch answered'
    '5' = 'watch disconnected, Recon still running'
    '6' = 'watch reconnected'
}
$seen = @{}
function Check([string]$k) {
    if ($script:seen.ContainsKey($k)) { return }
    $script:seen[$k] = $true
    Note ("checklist $k " + $checks[$k])
    Shout ("CHECK $k SEEN: " + $checks[$k] + ('  (' + $script:seen.Count + ' of ' + $checks.Count + ')'))
}

Say "Logging the LayerWand with serial $Serial to $logPath"
Note "serial $Serial"
$port = $null
$lastOpenError = ''
$bootCount = $null
$connected = $false
$disconnectedOnce = $false

try {
    while ($true) {
        if ($null -eq $port) {
            $name = Find-Port $Serial
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
            Say "Opened $name (serial $Serial)."
        }

        try {
            $line = $port.ReadLine().TrimEnd("`r")
        } catch [System.TimeoutException] {
            continue
        } catch {
            Note 'port lost'
            Alarm 'Port lost (replug or reset). Waiting for the LayerWand to come back.'
            try { $port.Close() } catch {}
            $port = $null
            Start-Sleep -Seconds 1
            continue
        }
        $writer.WriteLine($line)

        # "[periodic] reset <name> (<code>), boot N, ..." on every report.
        if ($line -match '^\[(periodic|boot|dump|run-end)\] reset (.+?) \((\d+)\), boot (\d+),') {
            $reason = $Matches[2]
            $n = [int]$Matches[4]
            if ($null -eq $bootCount) {
                $bootCount = $n
                Say "Boot $n, reset reason: $reason."
            } elseif ($n -ne $bootCount) {
                Note "boot count changed $bootCount -> $n reason $reason"
                Alarm "LAYERWAND RESET: boot $bootCount -> $n (reason: $reason). Logging continues on the new boot."
                $bootCount = $n
                $connected = $false
            }
        }

        if ($line -match '^Link connected to (\S+)') {
            if ($disconnectedOnce) { Check '6' } else { Check '2' }
            $connected = $true
            Say $line
        } elseif ($line -match '^Link disconnected') {
            $connected = $false
            $disconnectedOnce = $true
            Say $line
        } elseif ($line -match '^command (\d+) arg (\d+) result (\d+)') {
            if ($Matches[3] -eq '0') { Check '4' }
            Say $line
        } elseif ($line -match '^\[periodic\] mode (\S+) run (\d+) s .* frames (\d+) adverts (\d+) \| candidates (\d+) events (\d+)') {
            $frames = [long]$Matches[3]; $adverts = [long]$Matches[4]
            if (-not $connected -and ($frames + $adverts) -gt 0) {
                if ($disconnectedOnce) { Check '5' } else { Check '1' }
            }
            Say ("periodic: mode {0}, run {1} s, frames {2}, adverts {3}, candidates {4}, events {5}, watch {6}" -f $Matches[1], $Matches[2], $frames, $adverts, $Matches[5], $Matches[6], $(if ($connected) { 'connected' } else { 'not connected' }))
        } elseif ($line -match '^\[periodic\] link .* summaries (\d+) ') {
            if ([long]$Matches[1] -gt 0) { Check '3' }
        } elseif ($line -match '^(=== run (start|dump)|phase |event |early warning |Button press|LayerWand|Link (up|FAILED))') {
            Say $line.Substring(0, [Math]::Min(110, $line.Length))
        } elseif ($line -match 'CORRUPT|PSRAM allocation FAILED|NO RING|BLE FAILED') {
            Alarm $line
        }
    }
} finally {
    if ($port) { try { $port.Close() } catch {} }
    Note 'logger closed'
    $writer.Close()
    Write-Host ''
    Write-Host ('Checklist seen: ' + $seen.Count + ' of ' + $checks.Count + '.')
    foreach ($k in $checks.Keys) { Write-Host ('  ' + $k + ' ' + $(if ($seen.ContainsKey($k)) { 'SEEN   ' } else { 'not yet' }) + ' ' + $checks[$k]) }
    Write-Host "Log closed: $logPath"
}
