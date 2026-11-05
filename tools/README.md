# Tools

## dscprobe/

Source of **DscProbe**, the read-only kernel probe used to inspect the
display/DSC state of the graphics driver on a live session. See
[dscprobe/README.md](dscprobe/README.md).

## phone/

Helpers that live on the phone (in `C:\`), run as administrator:

| File | Purpose |
|---|---|
| `gpu_now.bat` / `gpu_now.ps1` | Manually enable the GPU device and poll its status into `C:\gpu_now.log` for 5 minutes. Does **not** disable it afterwards — disable it yourself after the test and verify `ConfigFlags` is back to 1 before rebooting (see [../STATUS.md](../STATUS.md)). |
| `kd_on.bat` | Enable kernel debugging (`bcdedit /debug on`) for a WinDbg/KDNET session. |
| `kd_off.bat` | Disable kernel debugging (the default). With debug on and no host attached, boot blocks waiting for the KD transport. |
