English | [日本語](README.ja.md)

# Tools

## dscprobe/

Source of **DscProbe**, the read-only kernel probe used to inspect the display/DSC state of the graphics driver on a live session. See [dscprobe/README.md](dscprobe/README.md).

## phone/

Helpers that live on the phone (in `C:\`), run as administrator:

| File | Purpose |
|---|---|
| `gpu_now.bat` / `gpu_now.ps1` | Manually enable the GPU device and poll its status into `C:\gpu_now.log` for 5 minutes. Does **not** disable it afterwards — disable it yourself after the test and verify `ConfigFlags` is back to 1 before rebooting (see [../STATUS.md](../STATUS.md)). |
| `kd_on.bat` | Enable kernel debugging (`bcdedit /debug on`) for a WinDbg/KDNET session. |
| `kd_off.bat` | Disable kernel debugging (the default). With debug on and no host attached, boot blocks waiting for the KD transport. |


## Capture workflow and cleanup

Read [known issues](../docs/known-issues.md) before enabling the GPU. Place both gpu_now files together in the phone root as assumed by the BAT wrapper; it starts the PowerShell script with elevation and logs thirty samples at ten-second intervals. It does not collect MMIO or replace DscProbe. Run the probe separately while the normal driver has powered and clocked the display.

After collecting evidence, disable the graphics instance, query ConfigFlags and verify 1 before reboot. If the session freezes before cleanup, inspect/repair the phone SYSTEM hive offline through gadget mode. A read-only probe cannot prevent the platform's processor-rendezvous freeze, and a five-minute status log is not a stability certification.

Keep logs, signed SYS files, build intermediates and certificates outside the repository. Do not post raw machine-specific logs without removing owner identifiers. Kernel-debug scripts change the phone's BCD; they do not create or configure a debugger connection.
