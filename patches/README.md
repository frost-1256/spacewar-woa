English | [日本語](README.ja.md)

# Driver patches for spacewar

This directory contains **patch specifications only** — file offsets and before/after bytes. No driver binaries (original or patched) are distributed here. You obtain the driver pack yourself, patch your own copy, and re-sign it with your own test certificate.

Machine-readable form: [`patches.json`](patches.json), consumed by [`apply_patches.py`](apply_patches.py). The script identifies the target by SHA-256, re-verifies every site's original bytes, backs up the file, and refuses anything unexpected.

```console
python3 apply_patches.py qcdxkm7325.sys dsc2
python3 apply_patches.py qcwlan7325.sys sarv2-allow
```

After patching, re-sign (the original signature no longer matches):

```console
signtool sign /s My /n "<your test cert>" /fd SHA256 qcdxkm7325.sys
```

All offsets below are **file offsets** in the pristine file identified by the given SHA-256. Variants are cumulative: `dsc1` includes `capw`, `dsc2` includes `dsc1`.

---

## qcdxkm7325.sys — graphics (SM7325 pack)

Pristine SHA-256: `fd3af19c0cb1792ac6727970e1335432fb5d41f72f9240375a95578a92685b8f`

### Variant `capw` — arming set

Without these, the driver does not survive start on spacewar: several checks that are soft failures on the donor device are hard traps here, one firmware poll is too short, and the shipped DSC capability table rejects the panel's 1080-wide DSC mode outright.

| Site | File offset | Original | Patched | Purpose |
|---|---|---|---|---|
| flv-nonfatal | 0x25C54 | `17 00 80 52` | `37 00 80 52` | FLV check made non-fatal (`mov w23,#0` → `#1`) |
| dsc-trap-1 | 0xA114C | `80 00 3E D4` | `29 00 80 52` | DSC trap (`brk`) → `mov w9,#1` |
| dsc-trap-2 | 0xA1520 | `80 00 3E D4` | `28 00 80 52` | DSC trap (`brk`) → `mov w8,#1` |
| caps-dsc-max-width | 0x1C9BD8 | `00 00 00 00` | `00 08 00 00` | DSC caps sub-struct +0x18: max width 0 → 2048 (data, .rdata) |
| perf-vote | 0x293738 | `09 05 00 51` | `49 00 80 52` | Performance vote forced (`sub w9,w8,#1` → `mov w9,#2`) |
| fw-init-poll-bound | 0x2938EC | `A0 86 01 00` | `40 42 0F 00` | Firmware-init poll bound 100000 → 1000000 |
| hfi-ack-timeout | 0x295BA0 | `80 69 67 FF` | `00 1F 0A FA` | HFI ack timeout path adjusted |

### Variant `dsc1` — DSC record hand-off fix

The mode-set path builds the DSC record at `S+0x124`, but the consumer copied it from `S+0xC8`, which is always zero — so the DSI side ran with an empty record (word count and enable state lost).

| Site | File offset | Original | Patched | Purpose |
|---|---|---|---|---|
| dsc-record-source | 0x123C50 | `82 22 03 91` | `82 92 04 91` | `add x2, x20, #0xc8` → `add x2, x20, #0x124` |

### Variant `dsc2` — COMPRESSION_MODE_CTRL2 (current on device)

The driver left DSC `COMPRESSION_MODE_CTRL2` at 0; the vendor stack programs the slice width (540). This patch supplies 540 at the register write. **Tested on device: the picture corruption remains unchanged**, so this is a correctness fix that matches the vendor value, not the fix for the display issue (see the project status).

| Site | File offset | Original (14 bytes) | Patched (14 bytes) | Purpose |
|---|---|---|---|---|
| ctrl2-slice-width | 0x125190 | `A9 01 0B 8B 2A 51 42 91 1F AD 02 B9 4C A9` | `8E 43 80 52 1F 20 03 D5 0E AD 02 B9 2C A9` | `mov w14,#540; nop; …` so CTRL2 receives 540 |

---

## qcwlan7325.sys — WLAN (SM7325 pack)

Pristine SHA-256: `09dd99c2ec0bac3f544ce8a468c66f92815e4c2d0369c7c3e25a85caa9d87f99`

### Variant `sarv2-allow` — SAR v2 must not fail initialisation

Nothing's stock firmware (`bdwlan.elf`) advertises SAR v2. On the v2 branch, `SarInit` returned `0xC0000001`, which failed the whole WLAN start. Returning success instead lets Wi-Fi come up; this is the patch that made Wi-Fi work on this device (with the INF change below).

| Site | File offset | Original | Patched | Purpose |
|---|---|---|---|---|
| sarinit-v2-success | 0x12C4E8 | `F3 0B 02 32` | `F3 03 1F 2A` | `orr w19, wzr, #0xc0000001` → `mov w19, wzr` (RVA 0x12D0E8, inside `SarInit`) |

### INF change — expected firmware version

The pack's INF gates start on an expected firmware version string. The donor device's firmware is `00228`; Nothing's stock firmware reports `00816`, and the driver refuses to start on mismatch. In `qcwlan7325.inf` (UTF-16LE):

```ini
; [Strings]
WlanFWVer = "00228"   →   WlanFWVer = "00816"
```

This value is written to the registry as `fwVersion` by `[fwVersion.reg]`. After editing the INF its catalog (`.cat`) no longer matches; regenerate/re-sign the affected catalog and use test signing with the trusted test identity, as with any modified driver package.


## Applying and interpreting the variants

Choose the final variant once on a **pristine** copy. The applicator accepts only a recorded pristine SHA-256, checks every original site before any change, resolves the cumulative chain, creates a timestamped backup and writes the result. A previously patched or re-signed file will be refused; return to the saved pristine input rather than removing the checks. `dsc2` includes all nine graphics sites (seven capw, one dsc1, one dsc2); `sarv2-allow` is independent and targets WLAN.

The patch specification and tables were generated from actual byte differences. The existing end-to-end check reproduced the dsc2 image byte-exact outside the signing region. Signature/certificate-table changes alter the final file hash, so do not compare a locally signed output's whole-file hash to an unrelated signed build and call the patch wrong. Record the pristine identity, selected variant and signing stage separately.

### Why the start patches exist

The FLV change makes the checked path non-fatal; it does not implement the donor board's missing condition. The two DSC trap replacements permit continuation where the original binary executes `brk`. The DSC capability data patch raises a zero maximum width to 2048 so the 1080-wide mode is accepted. The performance-vote change fixes the selected vote value, the firmware-init poll increases its bound, and the HFI acknowledgement patch adjusts that timeout path. These are the established arming set for startup; bypassing traps is not proof that every hardware requirement is satisfied. No fuller semantic claim about the HFI patch is established by the supplied table, so it is not described as a general timeout cure.

The dsc1 fix changes a **record source**, not a compression parameter: the builder fills one structure offset while the old consumer copies another zero region. That hand-off failure explains missing enable/byte-count data on the DSI side. dsc2 then supplies the vendor's CTRL2 slice width, but unchanged device output and later readback show it did not solve the structured noise. These results justify keeping a correctness fix without advertising a working display.

The WLAN patch changes the SAR v2 branch's return status only. It does not transform all v2 behavior into a verified implementation; the device's normal-speed start was demonstrated together with the INF firmware-version edit. See [subsystems](../docs/subsystems.md) for the two gates and [display investigation](../docs/display-investigation.md) for the graphics evidence.

## Package integrity and deployment order

The INF version edit is **not** a binary site in patches.json; perform it separately, preserving UTF-16LE. Finish all SYS/INF changes before regenerating catalogs and signing the completed package with your own phone-trusted test identity. Test signing enables acceptance of suitable test-signed code; it does not make a catalog with stale hashes correct. The example SYS signing command above is only the embedded-signature step, not the entire package-signing workflow.

Install using the [safe-baseline guide](../docs/install.md). Direct replacement of an installed protected DriverStore file is offline/gadget only, never a live overwrite. GPU enable may leave ConfigFlags=0, so disable and verify 1 before reboot after any test. No signing identity or driver binary belongs in this repository.

## Proposed patches are separate work

The four-instruction DSC-off candidate and the faithful-order PPS hook are **planned experiments**, not implemented variants here. They have panel/clock/safety conditions and must not be presented as additional verified patch sites. patches.json remains the complete machine-readable definition of the currently published variants.
