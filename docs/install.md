English | [日本語](install.ja.md)


# Installation and safe servicing

Target: Nothing Phone (1) A063 / SM7325, Windows 11 ARM64 **22H2**, current fork configuration as of 2026-11-05. Read [status](../STATUS.md) and [known issues](known-issues.md) before changing the device. This guide records the proven installation model; it does not claim a newly tested clean install or a universal partitioning recipe.

## 1. Prerequisites and recovery preparation

- An unlocked A063, a PC with fastboot, a reliable USB connection, and a way back to fastboot/recovery.
- Backups of Android data, the original partition table and any partition images needed to restore your chosen layout. Repartitioning userdata is destructive; unlocking may also erase data.
- `Mu-spacewar.img` from [the Mu-Silicium fork](https://github.com/frost-1256/Mu-Silicium), with board tables from [the Silicium-ACPI fork](https://github.com/frost-1256/Silicium-ACPI). DSDT v21 contains the proven Bluetooth Serial Bus revision change; use a fork build retaining that fix and the touch support.
- A Windows 11 ARM64 22H2 image, Windows servicing tools (DISM and BCDBoot), and the Qualcomm SM7325/A52s driver pack obtained separately.
- Python 3 for the patch applicator; signing tools and your own private test-signing identity for modified kernel binaries and catalogs.
- The [FT3680 touch driver](https://github.com/frost-1256/spacewar-ft3680), built/installed according to its instructions.

Keep pristine driver packages and the last known bootable firmware/package set outside the repository. Before experimenting, confirm gadget mode can expose the Windows volume and ESP while the phone's Windows is not running. Recovery must not depend on a working GPU, Wi-Fi or touch inside a frozen Windows session.

## 2. Storage layout and image deployment

The demonstrated layout uses space carved from userdata:

| Partition | Size / format | Purpose |
|---|---|---|
| ESP | 512 MiB / FAT32 | Windows UEFI boot files and BCD |
| WIN | 64 GiB / NTFS | Windows system volume |

The record does **not** preserve the original shrink/create commands, alignment decisions or a full before/after GPT. Do not execute a guessed DiskPart script or copy another phone's sector offsets. Determine the actual layout, backup and recovery procedure first. The sizes above describe a working layout, not minimum requirements or guaranteed free capacity.

With only the intended phone partitions mounted for servicing, assign verified temporary letters: the examples use `W:` for WIN and `S:` for ESP; `I:` is example installation media. These are placeholders, not discovered drive letters. Inspect the image and select its ARM64 edition index before applying it:

```bat
dism /Get-WimInfo /WimFile:I:\sources\install.wim
```

Replace `<ARM64_INDEX>` with the chosen index; if the media supplies ESD or split images, adapt DISM's input options rather than renaming files:

```bat
dism /Apply-Image /ImageFile:I:\sources\install.wim /Index:<ARM64_INDEX> /ApplyDir:W:\
bcdboot W:\Windows /s S: /f UEFI
```

These are standard servicing command templates, not a recovered transcript of the original installation. Verify the resulting BCD refers to the phone's Windows installation, then configure that store explicitly as below. Do not let a command intended for the phone change the servicing PC's BCD.

## 3. Boot model

From fastboot on each Windows boot:

```console
fastboot boot Mu-spacewar.img
```

This loads UEFI into memory; it does not flash the phone's boot slots. Android remains on its existing slot under the demonstrated arrangement. A reboot can return to Android/fastboot, but does not undo partition changes or repair a damaged GPT. Standalone A/B dual boot is a future idea; no flashing procedure is supplied here. Linux bring-up steps such as DTBO erasure are not part of this Windows installation procedure.

## 4. Test signing and debugger default

A changed SYS invalidates its embedded signature; changing either SYS or INF also invalidates package catalog hashes. Signing a SYS alone does not repair a stale catalog. Apply all byte/INF changes first, rebuild the affected catalogs using the appropriate WDK package tooling, then sign binaries/catalogs and verify the completed package before staging it. Do not reuse a mismatching original CAT.

Create your own test certificate and keep its private key on the signing system. The signing script uses `CurrentUser\My`; it does not require copying the private key to the phone. Import only the public certificate into the phone's Local Machine Trusted Root Certification Authorities and Trusted Publishers stores as required for that test identity. No certificate or key is distributed here.

On the running phone, use an elevated prompt:

```bat
bcdedit /set testsigning on
bcdedit /debug off
```

For offline setup, target the phone's store and inspect its loader entries first:

```bat
bcdedit /store S:\EFI\Microsoft\Boot\BCD /enum all
bcdedit /store S:\EFI\Microsoft\Boot\BCD /set {PHONE_LOADER_GUID} testsigning on
bcdedit /store S:\EFI\Microsoft\Boot\BCD /set {PHONE_LOADER_GUID} debug off
```

Substitute the actual Windows loader identifier from that store. Debug is **off by default**: the configured kernel-debug transport can wait for an absent debugger during initialization. `bootdebug` is a separate setting; changing it alone did not solve that wait. Use `kd_on.bat` only for an intentional debugger session and return to `kd_off.bat` afterward. Test signing must be compatible with the firmware's boot security configuration; the record does not supply a universal Secure Boot configuration recipe.

## 5. Prepare drivers in order

1. Preserve the pristine pack and check the two target hashes against [the patch specification](../patches/README.md). A different build requires a new audit; never bypass the applicator's refusal.
2. From a pristine WLAN SYS, apply `sarv2-allow`; edit `qcwlan7325.inf` in UTF-16LE so `WlanFWVer` changes from `00228` to `00816`. Both changes are needed: the version gate and SAR initialization are separate failures.
3. If investigating display, create `dsc2` directly from a pristine graphics SYS. It cumulatively includes `capw` and `dsc1`; do not run the applicator sequentially on an already patched file. The GPU is unnecessary for preserving the working touch/Wi-Fi/Bluetooth baseline.
4. Finish all INF edits, regenerate affected catalogs and re-sign modified packages with the phone-trusted test identity. Keep a record of input hash, selected variant, final hash and matching firmware/ACPI build.
5. Stage/install board providers and their extension INFs together with the intended pack configuration; do not assume copying an extension INF applies its settings. Install the separate touch package using its repository instructions.
6. Establish the GPU-disabled and ADSP-disabled baseline **before permitting their first startup**. Avoid indiscriminately enabling every device in the pack; boot-time ADSP is known to crash.

From the repository root, patch commands for your separately obtained files are:

```console
python3 patches/apply_patches.py qcwlan7325.sys sarv2-allow
python3 patches/apply_patches.py qcdxkm7325.sys dsc2
```

For initial offline provisioning, DISM can stage a deliberately prepared driver directory into the phone image:

```bat
dism /Image:W:\ /Add-Driver /Driver:<PREPARED_PACK_DIRECTORY> /Recurse
```

For normal supported package installation on the running phone, the existing workflow uses an elevated prompt in the prepared directory:

```bat
pnputil /add-driver *.inf /subdirs /install
```

`/install` may start matching devices immediately. Use it only after the disabled baseline is established; select packages deliberately rather than applying it blindly to GPU/ADSP during first boot. Check device bindings and extension applicability afterward. The source record supplies no universal total ordering for every donor-pack provider, so this guide does not invent one; the concrete ADSP constraint is SSDD registration **before** ADSP QMI use.

## 6. Set and verify the safe baseline

Graphics instance used by this board configuration:

```text
ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
```

On running Windows, disable it and inspect the current control set:

```bat
pnputil /disable-device "ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0"
reg query "HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags
```

Expected baseline is `ConfigFlags REG_DWORD 0x1`, with the device disabled in Device Manager. The demonstrated offline key was:

```text
HKLM\SYSTEM\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
  ConfigFlags (REG_DWORD) = 1
```

When editing offline, load the phone SYSTEM hive, inspect `Select\Current`/`Default` and target the control set used for its next normal boot; do not assume another installation uses ControlSet001. The full [offline recovery example](known-issues.md) includes backup, unload and readback. Keep ADSP boot-disabled too; identify its actual instance under `ACPI\QCOM0A1B` or the matching extended ID rather than guessing its instance suffix.

GPU tests are manual, preferably during a quiet sign-in-screen window, brief, and followed immediately by disable. `gpu_now.bat` only enables and logs for five minutes; it has no cleanup. `pnputil /enable-device` can persistently set ConfigFlags=0. **After every enable, query again before reboot; if a crash prevented disable, repair offline before another Windows boot.**

### Settings to recreate after a reinstall (confirmed during the 2026-11-07 recovery)

Re-adding drivers exported with `dism /Image:<WIN> /Export-Driver` does not restore the following, because they live in the SYSTEM/SOFTWARE hives. Keep the previous hives (with their log files) and port only these keys; restoring a whole hive copied without its logs can fail to load.

| Item | Required state | Notes |
|---|---|---|
| Boot-disabled devices | ConfigFlags=1 on GPU, ADSP (QCOM0A1B), AMSS (QCOM0A1C), QSM (QCOM0A1E), CDSP (QCOM0AB0) | A fresh hive has no instance keys; create them beforehand. Setup can recreate them with 0, so re-check after first boot |
| Service start types | `qci2c` Start=4, `cnghwassist` Start=4 | Re-adding the driver package resets `qci2c` to 3 |
| QcTrEE parameters | The previous `Services\QcTrEE\Parameters` (FSListener=0, QseeLogBuffer=0, only the three required SecureServices enabled) | The INF defaults (almost everything enabled) combined with the SCM-side drivers stopped the boot |
| PILC / WPSS | IMEM / PGCM / PilConfig / SubsystemLoad under `Enum\ACPI\QCOM06E0\…`; Mappings / Resources / PDInfo / Desktop under `Enum\ACPI\QCOM0AE2\…` | Needed for the Wi-Fi chain; hand-added values do not come back with the drivers |
| Wi-Fi firmware version gate | fwVersion=00816 on the WCN6750 subkey of the Net class {4D36E972-E325-11CE-BFC1-08002BE10318} | If the INF `WlanFWVer` is still 00228, override it in the class key |
| Power settings | No automatic display-off, no sleep, power/sleep button and lid actions = do nothing (active power plan) | `Control\Power\User\PowerSchemes` is SYSTEM-only; import with SYSTEM rights |
| Sleep Study | `Control\Power\SleepStudyDisabled`=1 | Avoids a 0xA0 (0x605) in `nt!PopPluginQuerySocSubsystemBlockingTime` on display power-on |
| Test signing | Public certificate of your test identity in ROOT and TrustedPublisher of the SOFTWARE hive | — |
| Local account | An account created by unattended setup may be refused network logon as "password expired" (4625, SubStatus 0xC0000071) | Set it to never expire and clear "must change at next logon" on the phone |

On the DSDT side, use a build in which WPSS depends (`_DEP`) on SSDD. Without it, qcsubsys can bugcheck 0x0A at boot ([known issues](known-issues.md) §3).

## 7. First-boot acceptance checklist

- Reaches logon and permits sign-in with kernel debugging off; a PIN can keep a deliberate sign-in window available for later tests.
- GPU is disabled and ConfigFlags reads 1; ADSP remains disabled for boot.
- Touch handles tap, release and long press, including after device re-scan (`pnputil /scan-devices`).
- Wi-Fi discovers networks and connects at normal speed; Bluetooth discovers, pairs and delivers headset audio.
- Storage, buttons and charging work; a missing battery percentage is still expected.
- A gadget-mode recovery pass can inspect the phone's BCD and SYSTEM hive without booting its Windows.

Record the OS version, firmware/DSDT identity, installed package versions and selected patches before display experimentation. No successful checklist item should be interpreted as a successful DSC display test.

## 8. Replacement and troubleshooting

**DriverStore replacement rule:** never directly overwrite a protected driver file in the running phone's DriverStore, even after disabling its device. A live replacement previously lost the file and required offline restoration. For the project's direct replacement workflow, shut down phone Windows, expose the volume in gadget mode, back up the relevant installed package, copy the correctly re-signed matching files offline, verify hashes and signatures, unmount cleanly, then boot. Use normal package servicing when installing a new package; do not mix SYS/INF/CAT from different revisions or edit all similarly named DriverStore folders by guesswork.

| Symptom | Check / response |
|---|---|
| Boot waits when no debugger is attached | Inspect the phone loader's `debug` setting; keep it off for normal boots |
| Freeze after a previous GPU enable | Before retrying Windows, inspect ConfigFlags offline; restore 1 if cleanup was interrupted |
| Freeze with GPU already disabled | 0x101 also occurs during heavy early boot; preserve evidence, verify flags and avoid repeated automatic enable loops |
| New package refuses to load | Check original hash/variant, SYS signature, regenerated CAT, INF encoding, phone trust stores and test-signing BCD entry |
| WLAN refuses start | Check both fwVersion `00816` and SAR v2 patch, plus actual extension/firmware binding |
| Bluetooth Code 10 | Confirm Serial Bus RevisionID correction retained in the embedded DSDT; inspect bytes rather than relying on iasl macro spelling |
| Touch breaks after scan | Confirm integrated GPIO52–55 self-restore driver; do not add raw runtime power votes |
| ADSP boot crashes with 0x0A | Return ADSP to boot-disabled; allow SSDD to complete before a later runtime test |
| Display is structured noise | This is the known unresolved dsc2 behavior; do not replace already audited constants at random |

If boot freezes, recover to gadget mode/fastboot, preserve available reports and check both GPU and ADSP startup state. A freeze-type watchdog reset may produce no new dump; inspect file timestamps so an older 0x0A dump is not mistaken for the newest failure. Detailed mechanisms and recovery limits are in [known issues](known-issues.md).
