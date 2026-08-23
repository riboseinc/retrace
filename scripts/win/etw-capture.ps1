# SPDX-License-Identifier: BSD-2-Clause
#
# etw-capture.ps1 -- scripted Windows kernel-truth capture
# (TODO.trace-profile/24). Starts an ETW trace session on the
# Microsoft-Windows-Kernel-File provider, runs the target, stops
# the session, and extracts file events to raw jsonl rows for
# retrace-etw2retrace.
#
# Requires an elevated shell (logman trace sessions). procmon
# (GUI -> CSV -> retrace-procmon2retrace) stays the
# zero-install path.
#
# usage:
#   powershell -File etw-capture.ps1 -Target .\app.exe `
#       [-TargetArgs '-v'] [-OutDir .\etw-out] [-Session retrace-etw]
#
# output:
#   <OutDir>\etw-events.jsonl   raw rows: {time,pid,tid,task,file,detail}
#   <OutDir>\etw-trace.etl      the raw ETL (keep for re-extraction)
#
# The row shape is pinned HERE (never a tool dialect), so the C
# converter sees one stable format across Windows versions.

param(
    [Parameter(Mandatory = $true)]
    [string]$Target,
    [string]$TargetArgs = "",
    [string]$OutDir = ".\etw-out",
    [string]$Session = "retrace-etw"
)

$ErrorActionPreference = "Stop"

if (-not ([Security.Principal.WindowsPrincipal] `
        [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Error "etw-capture requires an elevated shell (logman trace session)."
}

$Target = (Resolve-Path $Target).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$etl = Join-Path $OutDir "etw-trace.etl"
$jsonl = Join-Path $OutDir "etw-events.jsonl"

# A stale session from a previous crashed run blocks the name.
logman stop $Session -ets 2>$null | Out-Null
Remove-Item $etl -ErrorAction SilentlyContinue

logman create trace $Session -ow -o $etl -nb 16 256 `
    -bs 64 -p Microsoft-Windows-Kernel-File -ets | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "logman create trace failed (is Microsoft-Windows-Kernel-File available?)."
}

try {
    # Resolve the target's executable BEFORE the trace starts;
    # Start-Process touches the filesystem too.
    $pinfo = Start-Process -FilePath $Target `
        -ArgumentList $TargetArgs -PassThru -NoNewWindow
    $targetPid = $pinfo.Id
    $pinfo.WaitForExit()
}
finally {
    logman stop $Session -ets | Out-Null
}

# Extraction. CI evidence (TODO.trace-profile/24 round 2): on
# GitHub runners, Get-WinEvent decodes Kernel-File ETL events
# with EMPTY Message and TaskDisplayName (no manifest decode),
# so Message-regex extraction yields nothing. The raw payload
# does survive: $_.Properties (typed values) and $_.Id. Rows are
# pinned to {time,pid,tid,task,file,detail}:
#   task = TaskDisplayName, else "Id<N>" (converter maps known
#          names; Id-pinning below keeps unmapped ones skipped)
#   file = first string Property that looks like a path
#          (Kernel-File FileName is always absolute), else the
#          Message "FileName:" regex when a decode does happen.
$epoch = [datetime]::new(1970, 1, 1, 0, 0, 0, [datetimekind]::Utc)
$rows = Get-WinEvent -Path $etl -Oldest -ErrorAction SilentlyContinue |
    Where-Object { $_.ProcessId -eq $targetPid } |
    ForEach-Object {
        $ev = $_
        $file = $null
        $msg = "$($ev.Message)"
        if ($msg -match 'FileName:\s*(\S.*)$') {
            $file = $Matches[1].Trim()
        }
        if (-not $file) {
            foreach ($prop in $ev.Properties) {
                if ($prop.Value -is [string] -and
                    $prop.Value -match '^[A-Za-z]:\\|^\\Device\\') {
                    $file = $prop.Value
                    break
                }
            }
        }
        if (-not $file) { return }
        $task = "$($ev.TaskDisplayName)".Trim()
        if (-not $task) { $task = "Id$($ev.Id)" }
        $time = [math]::Floor(
            ($ev.TimeCreated.ToUniversalTime() - $epoch).TotalSeconds)
        [pscustomobject]@{
            time = $time
            pid = $ev.ProcessId
            tid = $ev.ThreadId
            task = $task
            file = $file
            detail = ""
        }
    }

if ($rows) {
    $rows | ForEach-Object {
        $_ | ConvertTo-Json -Compress -Depth 2
    } | Set-Content -Path $jsonl -Encoding ascii
    Write-Host ("etw-capture: {0} file events (pid {1}) -> {2}" -f `
        @($rows).Count, $targetPid, $jsonl)
} else {
    Set-Content -Path $jsonl -Value "" -Encoding ascii
    Write-Warning "etw-capture: no named file events captured for pid $targetPid."
    # Self-describing failure (the TODO 23 method): what the ETL
    # actually holds -- provider/task histograms and a pid
    # histogram, so a filter mismatch is visible at a glance.
    $all = @(Get-WinEvent -Path $etl -Oldest -ErrorAction SilentlyContinue)
    Write-Host "etw-diagnostics: total ETL events: $($all.Count)"
    $all | Group-Object ProviderName | Sort-Object Count -Descending |
        Select-Object -First 5 | ForEach-Object {
            Write-Host "etw-diagnostics: provider $($_.Name) x$($_.Count)"
        }
    $all | Group-Object ProcessId | Sort-Object Count -Descending |
        Select-Object -First 8 | ForEach-Object {
            Write-Host "etw-diagnostics: pid $($_.Name) x$($_.Count)"
        }
    # Id histogram + per-Id path samples: pins the Id->task
    # mapping (12/13/14/15/...) from REAL data -- manifest decode
    # is not available, so the numeric Id is the only semantics.
    $all | Group-Object Id | Sort-Object Count -Descending |
        Select-Object -First 10 | ForEach-Object {
            Write-Host "etw-diagnostics: id $($_.Name) x$($_.Count)"
        }
    $all | Select-Object -First 6 | ForEach-Object {
        $pv = ($_.Properties | ForEach-Object {
            if ($_.Value -is [string]) { "s:'$($_.Value)'" }
            elseif ($null -ne $_.Value) { "v:$($_.Value)" }
        }) -join " "
        Write-Host ("etw-diagnostics: sample id={0} pid={1} props={2}" -f `
            $_.Id, $_.ProcessId, $pv)
    }
}
