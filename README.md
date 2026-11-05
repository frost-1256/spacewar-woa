# Windows 11 on Nothing Phone (1) — spacewar

Notes, an install guide, binary patch specifications, and small tools from an
ongoing port of **Windows 11 ARM64 (22H2)** to the **Nothing Phone (1)**
(A063, Snapdragon 778G / SM7325, codename **spacewar**), built on a
Mu-Silicium UEFI fork.

> **Status in one line:** Windows boots to the logon screen and **touch,
> Wi-Fi and Bluetooth work**. Display output is the open problem: the GPU
> driver starts and drives the panel, but the DSC stream decodes as garbage,
> so the GPU is kept disabled at boot while that is investigated.
> Full inventory: [STATUS.md](STATUS.md).

## What's in this repository

| Path | Contents |
|---|---|
| [docs/install.md](docs/install.md) | End-to-end install walkthrough |
| [patches/](patches/) | Binary patch **specifications** (offsets + before/after bytes) and an apply script for the SM7325 driver pack |
| [tools/](tools/) | The DscProbe measurement driver (source) and phone-side helper scripts |
| [STATUS.md](STATUS.md) | What works, what doesn't, and known defects |

## What is *not* in this repository

- **No driver binaries** — neither original nor patched. The Qualcomm driver
  pack for SM7325 is not redistributed here; obtain it yourself and apply the
  patch specifications to your own copy.
- **No certificates or keys.** Patched drivers are re-signed with your own
  test certificate; the steps are in the install guide.

## Related projects (same author)

- [Mu-Silicium fork](https://github.com/frost-1256/Mu-Silicium) — UEFI
  firmware for this device (upstream spacewar support is inactive; this fork
  carries it to a booting Windows)
- [Silicium-ACPI fork](https://github.com/frost-1256/Silicium-ACPI) — the
  DSDT/ACPI tables (touch, Wi-Fi and Bluetooth fixes landed here)
- [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680) — the
  FocalTech FT3680 touch driver

## Warning

This is experimental work on a device that can wedge hard enough to need
fastboot/EDL recovery. The platform has a known intermittent freeze
(BugCheck 0x101) that the GPU work is actively chasing. If you follow any of
this, you do so at your own risk, on your own device.
