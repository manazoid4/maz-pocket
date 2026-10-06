# Start nod Flow agent at login (Startup shortcut, hidden pythonw).
$py = (Get-Command pythonw.exe -ErrorAction Stop).Source
$lnk = Join-Path ([Environment]::GetFolderPath("Startup")) "nodflow.lnk"
$s = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$s.TargetPath = $py; $s.Arguments = "`"$PSScriptRoot\nodflow.py`""; $s.WindowStyle = 7; $s.Save()
Write-Host "installed $lnk"
