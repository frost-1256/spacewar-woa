English | [日本語](README.ja.md)

# DscProbe

A one-shot, **read-only** kernel probe for Windows 11 ARM64, used to inspect the display/DSC state of `qcdxkm7325.sys` on spacewar while the display is live. It never writes to hardware, memory of the target driver, the registry, or the network; the only file it writes is its own report, `C:\Windows\DscProbe_out.txt` (plain `key=value` lines).

Current version: **v3.8**. Besides the register sets read by earlier versions (DSC encoder block, DSC/CTL/INTF state, word count, DSC record), it reads:

- The panel-info DSC fields: master enable, mode index, fixed bit-clock override, bytes-in-slice, slice count and raw DSC config block.
- The DSI error/status set: FIFO, ACK, PHY, timeout, mask, interrupt control and clock status, for comparison with working mainline Linux.

It uses fixed RVAs inside `qcdxkm7325.sys`; it is tied to the exact driver build identified in [`../../patches`](../../patches). Every dereference is bounds-checked and guarded, and results are flushed in stages so a later guard failure cannot lose earlier readings.

## Build

From an EWDK `LaunchBuildEnv` shell targeting ARM64:

```bat
build.bat
sign.bat   :: substitute your own test certificate in sign.bat
```

## Use

Copy `DscProbe.sys` to the phone (e.g. `C:\DscProbe\DscProbe.sys`), then in an administrator prompt, with the graphics device enabled and the display active:

```bat
sc.exe create DscProbe type= kernel start= demand binPath= C:\DscProbe\DscProbe.sys
sc.exe start DscProbe
type C:\Windows\DscProbe_out.txt
sc.exe stop DscProbe
sc.exe delete DscProbe
```


## Safety limits and interpreting a capture

Display-up is an **operator precondition**. Bounds checks, mapped-page tests and exception guards are not a hardware power/clock lock and cannot recover from a bus hang on an unclocked register block. The probe never maps physical MMIO, powers a block up, acknowledges/clears errors or injects patterns; do not run it as an automatic early-boot probe. Token progress is informational rather than proof of correct pixels. The current source reports `S_fields=unavailable` because a validated mode-structure pointer is not available.

Service start queues a one-shot worker; wait for the output's final `status=` rather than assuming `sc start` means capture completed. Staged reports show `status=in_progress stage=...`; guard failures report unavailable fields and a final error. A partial or old report is not a successful new sample. Copy the output elsewhere before the next run overwrites it, and record the driver variant, DSC on/off, firmware/ACPI identity and capture timing.

The v3.8 baseline of 2026-11-05 measured FIFO_STATUS=0x99991290 against working mainline 0x11111210. This is evidence of the HS-lane underflow bit group in that snapshot, not proof of causality. Other errors may already have been selectively cleared by the display driver; a zero register is not a clean-history certificate. See the [full investigation](../../docs/display-investigation.md) for mask meanings and the still-unperformed time-course/DSC-off/clear-and-watch experiments. Clear-and-watch requires separate code; DscProbe has no write interface.

Build outside the repository or keep all generated artifacts outside its tracked tree. `sign.bat` expects a private key in the signing user's certificate store; replace its example subject with your own. Import the public certificate where needed on the phone; no private key or certificate file should be committed. Test signing and package-signing requirements are described in [installation](../../docs/install.md).

Stopping/deleting the probe service does **not** disable the GPU. Disable graphics separately and verify ConfigFlags=1 before reboot; recover offline if a crash prevents that cleanup. The example report/output locations are generic tool defaults, not a requirement to reproduce any author's private directory layout.
