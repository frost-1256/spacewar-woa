English | [日本語](README.ja.md)

# DscProbe

A one-shot, **read-only** kernel probe for Windows 11 ARM64, used to inspect
the display/DSC state of `qcdxkm7325.sys` on spacewar while the display is
live. It never writes to hardware, memory of the target driver, the registry,
or the network; the only file it writes is its own report,
`C:\Windows\DscProbe_out.txt` (plain `key=value` lines).

Current version: **v3.8**. Besides the register sets read by earlier versions
(DSC encoder block, DSC/CTL/INTF state, word count, DSC record), it reads:

- the panel-info DSC fields (master DSC-enable flag, mode index, fixed
  bit-clock override, bytes-in-slice, slice count, the raw DSC config block),
- the DSI error/status register set (FIFO status, ACK error, PHY error,
  timeout, error interrupt mask, interrupt control, clock status), so a
  Windows session can be compared against a working mainline Linux session.

It uses fixed RVAs inside `qcdxkm7325.sys`; it is tied to the exact driver
build identified in [`../../patches`](../../patches). Every dereference is
bounds-checked and guarded, and results are flushed in stages so a later
guard failure cannot lose earlier readings.

## Build

From an EWDK `LaunchBuildEnv` shell targeting ARM64:

```bat
build.bat
sign.bat   :: substitute your own test certificate in sign.bat
```

## Use

Copy `DscProbe.sys` to the phone (e.g. `C:\DscProbe\DscProbe.sys`), then in
an administrator prompt, with the graphics device enabled and the display
active:

```bat
sc.exe create DscProbe type= kernel start= demand binPath= C:\DscProbe\DscProbe.sys
sc.exe start DscProbe
type C:\Windows\DscProbe_out.txt
sc.exe stop DscProbe
sc.exe delete DscProbe
```
