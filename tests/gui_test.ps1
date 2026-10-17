# End-to-end test of the AudioTester window: scans tests\data, reads the results box and checks it
# against expected.txt, then (optionally) launches a second copy with a relative path to check that
# the file is forwarded to the window that's already open.
# usage: powershell -File gui_test.ps1 -Exe <path to AudioTester.exe> [-Data <folder, e.g. a UNC path>] [-SkipForward]
param([Parameter(Mandatory)][string]$Exe, [string]$Data, [switch]$SkipForward)

Add-Type -TypeDefinition @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class W {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string t);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr a, string c, string t);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, StringBuilder l);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, int m, IntPtr w, IntPtr l);
    public static string GetText(IntPtr h) {
        int n = (int)SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);   // WM_GETTEXTLENGTH
        var sb = new StringBuilder(n + 1);
        SendMessage(h, 0x000D, (IntPtr)(n + 1), sb);                      // WM_GETTEXT
        return sb.ToString();
    }
}
"@

$data = if ($Data) { $Data } else { Join-Path $PSScriptRoot 'data' }
$expected = @{}
Get-Content (Join-Path $data 'expected.txt') | ForEach-Object { $a = $_ -split "`t"; $expected[$a[0]] = $a[1] }

function Wait-Summary($edit, $pattern, $seconds = 60) {
    $deadline = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $deadline) {
        $t = [W]::GetText($edit)
        if ($t -match $pattern) { return $t }
        Start-Sleep -Milliseconds 200
    }
    return $null
}

if ([W]::FindWindow('AudioTester', [NullString]::Value) -ne [IntPtr]::Zero) { throw 'Close any open AudioTester window first.' }
$proc = Start-Process -FilePath $Exe -ArgumentList "`"$data`"" -PassThru
$main = [IntPtr]::Zero
for ($i = 0; $i -lt 50 -and $main -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; $main = [W]::FindWindow('AudioTester', [NullString]::Value) }
if ($main -eq [IntPtr]::Zero) { throw 'AudioTester window did not appear.' }
$edit = [W]::FindWindowEx($main, [IntPtr]::Zero, 'Edit', [NullString]::Value)

$failures = 0
$text = Wait-Summary $edit 'scanned in [\d.]+ seconds[\s\S]*passed'
if (-not $text) { Write-Output 'FAIL: no summary within 60 seconds'; $failures++ }
else {
    $lines = $text -split "`r`n"
    Write-Output ($lines[0])
    $section = ''
    $seen = @{}
    foreach ($line in $lines) {
        if ($line -match '^\d+ files? failed$') { $section = 'FAIL'; continue }
        if ($line -match '^\d+ files? passed$') { $section = 'PASS'; continue }
        if (-not $section -or -not $line -or $line -eq '---') { continue }
        $name = Split-Path (($line -split "`t")[0]) -Leaf
        $seen[$name] = $section
        $ok = $expected[$name] -eq $section
        if (-not $ok) { $failures++ }
        Write-Output ('{0,-4} {1}' -f $(if ($ok) { 'ok' } else { 'XX' }), $line)
    }
    foreach ($name in $expected.Keys) { if (-not $seen.ContainsKey($name)) { Write-Output "XX   missing from results: $name"; $failures++ } }
}

if (-not $SkipForward) {
    # Second copy, started from the data folder with a relative path: it should hand the file to the open window and exit
    $second = Start-Process -FilePath $Exe -ArgumentList 'good.flac' -WorkingDirectory $data -PassThru
    $text = Wait-Summary $edit '^1 file scanned' 20
    $second.WaitForExit(5000) | Out-Null
    if ($text -and $text -match 'good\.flac') { Write-Output 'ok   second launch forwarded good.flac (relative path) to the open window' }
    else { Write-Output 'XX   second launch did not forward its file'; $failures++ }
}

[W]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null   # WM_CLOSE
$proc.WaitForExit(10000) | Out-Null
Write-Output "GUI test mismatches: $failures"
exit $failures
