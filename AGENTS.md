English | [日本語](AGENTS.ja.md)

# AGENTS.md — working on spacewar-woa

Guidance for AI agents (and humans directing them) reading or modifying this
repository. This file is the fastest way to avoid re-deriving — or worse,
re-opening — two months of device investigation.

## What this repository is

Documentation, machine-readable binary **patch specifications**, a
hash-checking patch applicator, a read-only kernel probe source, and small
phone-side helper scripts for running **Windows 11 ARM64 (22H2)** on the
**Nothing Phone (1)** (A063, "spacewar", SM7325).

It contains **no driver binaries** (original or patched), no firmware, no
certificates or keys, and no Windows images. Companion repositories hold the
other layers: `frost-1256/Mu-Silicium` (UEFI firmware fork),
`frost-1256/Silicium-ACPI` (ACPI tables / DSDT), `frost-1256/spacewar-ft3680`
(touch driver source).

## Ground truth (snapshot 2026-11-05)

- **Working baseline:** boot to logon (UEFI loaded via `fastboot boot`, not
  flashed), touch, Wi-Fi at normal speed, Bluetooth (discovery, pairing,
  audio), USB gadget mode, charging, physical buttons, UFS storage.
- **The display is NOT working.** The graphics driver build `dsc2` starts and
  drives the Visionox RM692E5 panel, but the Display Stream Compression (DSC)
  stream decodes as structured noise. Never write, imply, or "fix" text to
  say the display works, is usable, or is close to working.
- **GPU is disabled at boot on purpose** (`ConfigFlags=1`) and is enabled
  manually only for short tests. This is a safety baseline, not a bug.
- **Independent platform defect:** BugCheck 0x101 — two processors can enter
  a mutual inter-processor rendezvous that never completes; the counterpart
  is arbitrary system activity. See `docs/known-issues.md`.
- **ADSP** crashes if started at boot (start-order defect, root cause
  identified); runtime enable after sign-in works. Battery devices start but
  Windows still reports no battery.

## Evidence discipline

Investigation text uses five labels: **PROVEN**, **EXCLUDED**, **INFERENCE**,
**OPEN**, **PLANNED**. Preserve them exactly.

- Never upgrade a claim beyond its evidence (a hypothesis stated in a source
  stays a hypothesis here — e.g. the FIFO-underflow group below, the URS0
  `_DEP` theory).
- A driver reaching "Started", a mode-set succeeding, a register matching, or
  a photograph of noise are each different evidence kinds. Do not merge them.
- Read-only snapshots are not wire traces. A register read of 0 after the
  fact is not proof an error never occurred (drivers read-and-clear status).
- When new results supersede old ones, say so explicitly; do not silently mix
  evidence from different dates or driver builds. State the build (`capw` /
  `dsc1` / `dsc2`) and the date with any measurement.

## Closed lines — do not re-investigate or re-propose

Register configuration as the cause of the display noise is **fully audited
and closed**. Specifically settled (evidence in
`docs/display-investigation.md`):

- PPS / encoder parameter mismatch (128-byte PPS, all 54 fields, agree
  host = panel = Linux; 32-register encoder evaluation matches).
- CTRL2 = 0 (fixed by `dsc2`; picture unchanged; live readback 540).
- Packetizer constants (vendor formulas reproduce the Windows values).
- Missing active DSC register writes (31/31 vendor writes have counterparts).
- CMN_MAIN_CNF reading as zero (writer proven; working mainline reads 0 too).
- DF_CTRL / initial_lines (Windows 0x095C0004 = vendor formula output;
  mainline's 0x095C0003 is a different derivation; panel accepts both).
- Topology / merge mismatch (working mainline is 1:1:1:1, same as Windows).
- INTF CONFIG2 + STREAM0_TOTAL pair (consistent tuple written and read back
  live; picture unchanged). CONFIG2 also varies between sessions on its own.
- HS timer difference (both stacks use fixed timeout constants).
- One slice per packet (v25: geometry changed, picture not fixed, stability
  worse) — do not re-run.
- Adding the compression-mode packet DT=0x07 via PCFG insertion, in both
  orders (v26/v27). The *faithful final-sequence* variant via a driver hook
  is a different, still-open experiment — do not conflate them.
- "Windows never reads DSI errors" (refuted by static audit).

## Open questions (where new work is useful)

1. **DSC framing defect** — the compressed stream is misframed somewhere
   outside the audited register configuration. Escalation ladder, in order:
   (1) DSC-off uncompressed experiment (analyzed, conditionally feasible,
   not implemented), (2) faithful-order PPS → DT=0x07 → 28 ms driver-hook
   experiment (RVA 0x10FA70), (3) test-pattern injection bisection,
   (4) display-only driver (last resort, months-scale).
2. **DSI FIFO underflow group** — Windows capture has the HS-lane underflow
   group 0x88880000 set in FIFO_STATUS (0x99991290 vs mainline 0x11111210);
   the group is unclassified/uncleared/unmasked in the driver. Neither cause
   nor normal — undecided. Discriminators: time-course capture across an
   enable; comparison under a DSC-off build; clear-and-watch.
3. **Frame completion** — frames do not reliably complete (separate from the
   framing defect).
4. **ADSP start order / battery reporting** — a mechanism for ADSP present
   from early boot, then whether the battery chain reports.
5. **USB host / PD** — URS0 not enumerated; `_DEP`-on-UCSI is a hypothesis.
6. **Panel-side DSC-off values** — empirical only. No public RM692E5 DDIC
   datasheet exists (searched 2026-11-05); do not invent register meanings
   by analogy to other panels.

## Invariants — never break these

- `patches/patches.json` and every offset / byte / SHA-256 value in the
  patch tables are **machine-generated from real binary diffs and verified
  end-to-end** (a `dsc2` build reproduced byte-exact outside the signing
  region). Never retype, "correct", or regenerate them from prose sources.
  A real change requires re-diffing the actual binaries and re-verifying
  with `apply_patches.py`.
- Distinguish the three address kinds and never mix them: **file offsets**
  (patch tables), **RVAs** (disassembly references, relative to the pristine
  graphics image), **physical MMIO addresses** (register captures).
- Never commit binaries, certificates, keys, device images, logs, or
  generated probe output. The `.gitignore` protections stay.
- Never add personal or infrastructure information (addresses, hostnames,
  accounts, remote-access setup). These documents describe the device and
  the software only.
- English and Japanese documents are full equivalents. Update both, keep
  the language switcher line at the top, and bump snapshot dates when the
  facts change. Well-known abbreviations (PC, UEFI, USB, DSC after first
  expansion) stay as-is in Japanese text.

## Hardware safety rules (if tests on a real device are ever run)

- After **every** GPU enable: disable again and verify `ConfigFlags=1`
  **before any reboot**. `pnputil /enable-device` can leave `ConfigFlags=0`
  persistently; if a session crashes first, the GPU starts at the next boot
  and can freeze it.
- Keep enable sessions short — the noise state heats the device.
- Enable only in a quiet window (sign-in screen, before sign-in), never
  during a signed-in active session.
- Replace DriverStore files only offline via the gadget-exposed volume.
  Never overwrite a live DriverStore file from a running session.
- Kernel debugging stays OFF by default (`bcdedit /debug off`); with it on
  and no host attached, boot waits on the debug transport.
- Freeze-type crashes may leave **no dump at all**; absence of a new dump is
  not absence of a crash. Check timestamps before attributing an old dump.
- Do not bounce battery / PMIC-GLink devices with `pnputil /disable-device`
  (they refuse runtime stop) and do not perform live register writes to
  unclocked blocks.

## Document map

| File | Owns |
|---|---|
| `README.md` | Overview, repository boundaries, related projects |
| `STATUS.md` | Subsystem inventory with mechanisms / root causes |
| `docs/install.md` | Reproduction steps, signing, patch order, troubleshooting |
| `docs/display-investigation.md` | The display/DSC evidence record (flagship) |
| `docs/known-issues.md` | 0x101 freeze, ConfigFlags trap, ADSP crash, dump caveats |
| `docs/subsystems.md` | Touch / Wi-Fi / Bluetooth / ADSP / USB notes |
| `patches/` | Patch specifications + applicator (values are authoritative) |
| `tools/` | DscProbe source, phone-side helper scripts |

If a fact seems to be missing, check `docs/display-investigation.md` §10
(provenance) before assuming it is unknown — and if it is genuinely not in
the record, mark it OPEN rather than filling the gap from general knowledge.
