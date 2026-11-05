English | [日本語](README.ja.md)


# Windows 11 on Nothing Phone (1) — spacewar

Public documentation of Windows 11 ARM64 **22H2** on Nothing Phone (1), model **A063**, codename **spacewar**, Snapdragon 778G / **SM7325**, using the author's Project Silicium forks.

**Status, 2026-11-05:** Windows boots to logon with UEFI loaded through `fastboot boot`; touch, normal-speed Wi-Fi, Bluetooth, USB gadget mode, charging, buttons and UFS storage work. The `dsc2` graphics driver starts and drives the panel, but its Display Stream Compression (DSC) output decodes as structured noise: usable driver-driven display is still unresolved. GPU boot startup is disabled, and an independent platform processor-rendezvous freeze (BugCheck 0x101) makes experiments costly. Audio, battery reporting and several other phone functions remain unavailable.

## Read in this order

| Document | Purpose |
|---|---|
| [STATUS.md](STATUS.md) | Complete subsystem inventory and limits of the working baseline |
| [docs/install.md](docs/install.md) | Prerequisites, image deployment, signing, patch order and safe first boot |
| [docs/known-issues.md](docs/known-issues.md) | Freeze mechanism, persistent ConfigFlags trap, ADSP ordering and recovery |
| [docs/display-investigation.md](docs/display-investigation.md) | Evidence, register comparisons, exclusions and planned experiments |
| [docs/subsystems.md](docs/subsystems.md) | Touch, Wi-Fi, Bluetooth, ADSP, battery and USB details |
| [patches/README.md](patches/README.md) | Verified patch tables, cumulative variants and package changes |
| [tools/README.md](tools/README.md) | Phone helpers and their operational limits |
| [tools/dscprobe/README.md](tools/dscprobe/README.md) | Source-only read-only display probe, build and capture procedure |

Every document has a full Japanese counterpart, linked at its top. Evidence labels in the investigation distinguish **PROVEN**, **EXCLUDED**, **INFERENCE**, **OPEN** and **PLANNED**; a successful driver start does not certify correct pixels or a stable platform.

## Repository boundaries

Included: documentation, machine-readable binary patch specifications, a hash-checking patch applicator, DscProbe source and build helpers, and small phone-side scripts.

Excluded: original or patched Qualcomm driver binaries, firmware bundles, Windows images, generated probe output, signing certificates and private keys. Obtain the driver pack and operating system separately; build and sign your own outputs. Patch applicability is defined by the pristine hashes in [patches.json](patches/patches.json), rather than a filename or the general label “SM7325 pack”. Keep build outputs, logs and backups outside this repository; the existing ignore protections remain in place.

## Related projects and responsibility split

- [frost-1256/Mu-Silicium](https://github.com/frost-1256/Mu-Silicium): UEFI firmware fork used for this phone. Upstream lists spacewar as Inactive; the booting Windows result described here belongs to this fork's configuration.
- [frost-1256/Silicium-ACPI](https://github.com/frost-1256/Silicium-ACPI): ACPI board description, including the Serial Bus descriptor revision fix shipped in DSDT v21.
- [frost-1256/spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680): FocalTech FT3680 touch driver with GPIO mux self-restore.

ACPI fixes, the separate touch driver and driver-pack binary patches are different layers; applying the patch JSON alone does not reproduce the full board configuration.

## Experimental-device warning

Back up device data and the partition layout before repartitioning. This is an experimental port requiring an unlocked bootloader and a known recovery route; freezes may require fastboot or EDL recovery and can leave no dump. Loading UEFI into RAM does not make earlier partition changes reversible. The installation guide preserves the current `fastboot boot` model and does not instruct flashing UEFI or erasing Android boot metadata.

After **every GPU enable**, disable the device and verify **ConfigFlags=1 before reboot**. A crash can interrupt cleanup and leave ConfigFlags=0 persistently, making the next boot start the GPU again. See the [recovery procedure](docs/known-issues.md).

## Longer-term ideas — not started

Standalone Android/Windows dual boot through A/B slots, support for later Windows major versions after 22H2 stabilizes, and a clean-room open-source reimplementation of Qualcomm drivers are future ideas. None is a supported installation route or a promised deliverable.
