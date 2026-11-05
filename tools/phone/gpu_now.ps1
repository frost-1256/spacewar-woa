$ErrorActionPreference = 'Continue'
$log = 'C:\gpu_now.log'
$id = 'ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0'
function L([string]$s) {
  $line = ('[{0}] {1}' -f [DateTimeOffset]::Now.ToUnixTimeMilliseconds(), $s)
  [System.IO.File]::AppendAllText($log, $line + "`r`n")
}
L '=== GPU NOW start (idle-enabled config) ==='
$d0 = Get-PnpDevice -InstanceId $id -ErrorAction SilentlyContinue
L ('before: Status=' + $d0.Status + ' Problem=' + $d0.Problem)
& pnputil /enable-device $id 2>&1 | ForEach-Object { L ('enable: ' + $_) }
for ($i = 1; $i -le 30; $i++) {
  Start-Sleep -Seconds 10
  $d = Get-PnpDevice -InstanceId $id -ErrorAction SilentlyContinue
  L ('tick ' + $i + ' Status=' + $d.Status + ' Problem=' + $d.Problem)
}
L '=== GPU NOW survived 300 s ==='
