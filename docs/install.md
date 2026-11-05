English | [日本語](install.ja.md)

# Install guide — Windows 11 ARM64 on Nothing Phone (1)

> **Draft.** Assembled from the project's working notes. The overall flow is
> proven on the author's device; exact command lines for your environment
> (drive letters, image indexes) will differ. Read
> [../STATUS.md](../STATUS.md) first so you know what you are getting.

## 0. What you need

- Nothing Phone (1) (A063) with an unlocked bootloader, and a PC.
- The UEFI image from the [Mu-Silicium fork](https://github.com/frost-1256/Mu-Silicium)
  (its CI builds `Mu-spacewar.img`; it embeds the DSDT from the
  [Silicium-ACPI fork](https://github.com/frost-1256/Silicium-ACPI)).
- A Windows 11 ARM64 (22H2) install image.
- The Qualcomm SM7325 driver pack (the "A52s pack"). Not distributed here —
  obtain it yourself.
- A code-signing test certificate of your own (self-signed is fine), trusted
  by the phone's Windows install.
- The touch driver from
  [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680).

## 1. Partitions and Windows install

The layout used on the author's device, carved out of the userdata area:

| Partition | Size | Contents |
|---|---|---|
| ESP | 512 MiB, FAT32 | UEFI-booted Windows boot files |
| WIN | 64 GiB, NTFS | Windows itself |

Apply the Windows image to WIN with DISM and create the boot files on ESP
with `bcdboot`, in the usual WoA fashion. (This install predates this
document; treat the partitioning step as environment-specific.)

## 2. Boot the UEFI

The UEFI is **not flashed**. Boot it from the PC each time:

```console
fastboot boot Mu-spacewar.img
```

Android stays in its slot untouched; if Windows wedges, rebooting returns
you to Android/fastboot as usual.

## 3. Test signing

Patched and home-built drivers require test signing:

- Create your own test certificate, install it into the phone's
  `CurrentUser\My` (for signing) and the machine's trusted stores.
- Enable test signing in the BCD (`bcdedit /set testsigning on`).
- Keep kernel debugging **off** by default (`bcdedit /debug off`); with it
  on, boot waits for a KD host that is not there. Use `tools/phone/kd_on.bat`
  only for a debugging session.

## 4. Driver pack and patches

1. Apply the patch specifications to your copy of the pack **before**
   installing the affected drivers (see [../patches/](../patches/)):
   - `qcwlan7325.sys` → variant `sarv2-allow`, plus the `fwVersion` INF edit
     (`00228` → `00816`) — these two make Wi-Fi start.
   - `qcdxkm7325.sys` → variant `dsc2` if you want to join the display
     investigation; the GPU is not needed for a working touch/Wi-Fi/
     Bluetooth system.
2. Re-sign every patched driver (and any driver whose INF you edited) with
   your test certificate.
3. Install the pack with `pnputil /add-driver *.inf /subdirs /install` from
   an administrator prompt, and the touch driver from its own repository.

Already-installed protected drivers are replaced **only** by writing the
re-signed file into the DriverStore through the offline (gadget-mode) volume
— never by overwriting a live DriverStore file from a running session.

## 5. GPU baseline: keep it off at boot

Out of the box, set the graphics device's `ConfigFlags` to 1 (disabled at
boot):

```text
HKLM\SYSTEM\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0
  ConfigFlags (REG_DWORD) = 1
```

Enable it only manually, in a quiet window at the sign-in screen, for
display experiments (`tools/phone/gpu_now.bat`), and disable it again right
afterwards. **After every enable, confirm `ConfigFlags` is 1 before the next
reboot** — an interrupted enable leaves it at 0 and the next boot will try to
start the GPU during boot, which can freeze. See
[../STATUS.md](../STATUS.md) for the defect behind this.

## 6. First boot checklist

- Boots to the logon screen; sign in works (a PIN is recommended — it also
  makes the boot stop at the sign-in screen, which is the quiet window used
  for GPU tests).
- Touch works, including after a `pnputil /scan-devices` (the driver
  restores its own pin mux).
- Wi-Fi lists networks and connects at normal speed; Bluetooth pairs and
  plays audio.
- Device Manager shows the graphics device disabled.
