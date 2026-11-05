English | [日本語](display-investigation.ja.md)


# Display and DSC investigation

Snapshot: **2026-11-05**, Nothing Phone (1) / spacewar, Windows 11 22H2, `qcdxkm7325.sys` **dsc2**. The display is **not working correctly**. Register configuration has been audited and closed as the explanation for the persistent noise; the open work concerns framing, sequence/timing and the DSI error domain. This document preserves the evidence needed to understand that boundary without private working notes.

## 1. Terms and evidence levels

Display Stream Compression (DSC) encodes pixels in hardware. The Picture Parameter Set (PPS) tells the panel decoder how to interpret them. The Display Serial Interface (DSI) host packetizes and serializes that stream. A layer mixer (LM), pingpong block (PP), control block (CTL), DSC encoder and interface block (INTF) form the display pipeline; passing mode-set or seeing a moving scan counter does not establish correct compressed payload delivery.

- **PROVEN:** directly observed on the phone, or established by identified source/disassembly/byte comparison; each claim identifies which kind.
- **EXCLUDED:** a specific proposed explanation has contrary evidence or a discriminating test failed to change the symptom; scope matters.
- **INFERENCE:** an explanation consistent with evidence but not directly demonstrated.
- **OPEN:** the evidence cannot yet decide.
- **PLANNED:** analyzed or proposed work that has not been implemented/tested.

All driver code locations here are **RVAs**, relative to the identified pristine graphics image, unless explicitly labeled file offsets. They are not physical MMIO addresses. Patch offsets and bytes belong exclusively to the preserved [patch tables](../patches/README.md). Read-only captures are not wire traces and do not prove that the panel accepted every command.

## 2. Panel, visible symptom and baseline

| Property | Recorded configuration |
|---|---|
| Panel / controller | Visionox panel using Raydium RM692E5 |
| Active size / link mode | 1080×2400, DSI command mode |
| Compression | DSC 1.1, SCR revision 0 |
| Component depth / compressed rate | 10 bits/component, 8 bits/pixel |
| Slice / packing | 540×20, two slices per packet, two slices per full-width line |
| Prediction / conversion | Block prediction and RGB conversion enabled; no native 4:2:2/4:2:0 |
| Windows driver | dsc2 = capw + dsc1 + CTRL2 slice-width correction |
| Safe baseline | GPU disabled for boot; manual, short runtime tests |

**PROVEN:** the driver starts and drives the panel, scanline activity advances, but compressed output becomes structured noise. A black screen is a different observation (transfer may not have started); it must not be merged with noise as one symptom. Photograph measurements found noise in rows 0 to about 580, a seam near x=541, a 20-row period in the left slice column and a 60-row period on the right, with repetition beginning around row 164. Slice alignment constrains possible framing faults; it does not identify a particular byte-order or padding bug.

The enabled/noise state heats the device, so tests end with prompt disable. A quiet sign-in-screen enable reduces background activity but cannot eliminate the independent 0x101 rendezvous freeze. Follow [known issues](known-issues.md), especially the persistent ConfigFlags check.

## 3. How the evidence accumulated

| Stage | Result | Consequence |
|---|---|---|
| capw | Removes fatal start barriers, extends polling and permits 1080-wide DSC capability | Driver startup becomes possible; no claim of correct display |
| dsc1 | Producer record at `S+0x124` is passed instead of always-zero `S+0xC8` | Restores word count and enable record reaching DSI |
| PPS / encoder audit | 128-byte PPS agrees across host, panel copy and Linux data; all 54 fields agree; 32-register encoder evaluation matches PPS | Encoder-parameter mismatch excluded |
| dsc2 | Writes CTRL2 slice width 540 | On-device noise unchanged; retained as vendor-matching correction |
| Vendor reference audit (GX-R7) | Packetizer constants derived; CONFIG2, common wrapper and routing questions isolated | Source-derived values kept separate from live Android reads |
| Write-list audit (GX-R8) | All 31 vendor active DSC writes have Windows counterparts; CMN writer and PP0 selector traced | No missing active DSC write established |
| Live tuple experiment, 2026-11-04 | CONFIG2=0x1111 and widebus-consistent STREAM0_TOTAL written and read back, held for minutes | Pair inconsistency excluded as the noise's cause |
| Working mainline harvest, 2026-11-05 | Correct display with single-block topology; raw register and clock capture | CMN read-zero and DF_CTRL questions closed |
| DscProbe v3.8 baseline, 2026-11-05 | DSC still on; Windows HS-lane FIFO-underflow group set | New concrete observation in the still-open DSI error domain |

Earlier notes calling DF_CTRL, topology, CMN readback or the absence of an error probe “open” predate these later results. Older conclusions about a universally absent compression-mode packet are also narrowed below; no historic source audit is treated as a wire capture.

## 4. Settled and excluded explanations

| Proposed explanation | Current disposition and one-line evidence |
|---|---|
| Host PPS differs from panel parameters | EXCLUDED: full 128-byte comparison / 54 fields agrees with panel copy and Linux data |
| Encoder parameters differ from PPS | EXCLUDED: 32-register emulation agrees, including rate-control thresholds/ranges |
| CTRL2=0 is the remaining fault | EXCLUDED: dsc2 supplies 540; unchanged picture, later live readback confirms 540 |
| Wrong packetizer constants | EXCLUDED: vendor formulas give CTRL=0x3901, CTRL2=540, WC=1081, STREAM0_CTRL=0x04390039 |
| Missing active DSC register write | EXCLUDED: 31/31 vendor active-mode writes have Windows counterparts |
| CMN_MAIN_CNF not programmed because it reads 0 | EXCLUDED: Windows writer supplies 0x100; working mainline also reads 0 |
| Wrong PP input mux because it reads 0 | EXCLUDED: PP0 active selector is 0; disabled vendor selector 0xF would be wrong here |
| Wrong multi-block / merge topology | EXCLUDED for the proposed Fairphone-style mismatch: working mainline uses LM0/PP0/CTL0/DSC0, 1:1:1:1, same shape as Windows |
| CONFIG2 / TOTAL width pair causes the noise | EXCLUDED: consistent live tuple written/read back without picture change |
| DF_CTRL initial_lines is wrong | CLOSED: Windows 0x095C0004 matches vendor formula; mainline 0x095C0003 reflects different derivation and panel accepts both |
| HS timer exposes a different pixel/bit clock assumption | EXCLUDED: both Windows and vendor writers use fixed timeout counts, with no mode-clock input |
| One slice per packet fixes decode | EXCLUDED as a fix: v25 changed pattern reproducibly, did not fix it and reduced survivability |
| Simply add DT=0x07 | EXCLUDED as the simple fix tested: v26/v27 insertion orders did not correct decode; faithful final sequence remains a distinct experiment |
| Windows never reads DSI errors | REFUTED: classification, selective clear and mask operations exist; specific omissions remain |

“Configuration closed” does not mean every register in two different operating systems must be identical, or that bytes on the wire have been verified. It means the proposed wrong/missing active configuration explanations were audited to closure. The transport's temporal behavior and compressed-byte framing are different questions.

### Packetizer arithmetic

For width 1080, slice width 540 and 8 compressed bits/pixel: two slices/interface; 540 bytes/slice; 1080 bytes/line and bytes/packet; one packet/line; `1080 % 3 = 0` end-of-line padding. The DCS memory-write byte makes word count **1081**. Ten bits/component affects PPS/encoder configuration, not this compressed byte-count calculation.

```text
CTRL (VC0)      = 0x3901
CTRL2 (VC0)     = 540 = 0x021C
STREAM0_CTRL    = (1081 << 16) | 0x39 = 0x04390039
TOTAL wide bus  = (2400 << 16) | ceil(1080 / 6) = 0x096000B4
TOTAL narrow    = (2400 << 16) | ceil(1080 / 3) = 0x09600168
DCS_CMD_CTRL    = 0x00013C2C  (start 0x2C, continue 0x3C, insertion enabled)
```

CONFIG2 bit0 is widebus, bit12 compressed 64-bit input for the relevant generation, bit4 DATA_HCTL_EN; command enable preserves other state. The vendor update is not a universal full-register constant: with the observed preserved baseline it yields 0x1111. Bit8's meaning was not established by that source audit. The experiment tested the complete relevant tuple, rather than assuming one full-register constant must always apply.

### Complete active-write audit

GX-R8 traced the identified Windows encoder leaf at RVA `0xDD4D0`: common setup → DF_CTRL → parameters/thresholds → input mux → wrapper configuration → DATA_IN_SWAP. The common store at `0xDD578` is reached through the successful mode-set path; it writes 0x100 for the saved two-slice, non-split inputs. The table below preserves the derived register list; it is **not a patch list**, and vendor source subblock bases were normalized to this layout.

| DSC offset | Register | Vendor value | Windows value | Windows store RVA | Vendor source |
|---|---|---|---|---|---|
| +0x000 | CMN_MAIN_CNF | 0x00000100 | 0x00000100 with zero readback; RMW formula in Q1 | 0xDD578 | dsc12:141–155 |
| +0x100 | ENC_DF_CTRL | 0x095C0004 if native422-capable HW; otherwise 0x12BC0004 | 0x095C0004 | 0xDD5D4 | dsc12:60–79,157–163 |
| +0x130 | MAIN_CONF | 0x101202DA | 0x101202DA | 0xDD634 | dsc12:165–179 |
| +0x134 | PICTURE_SIZE | 0x09600438 | 0x09600438 | 0xDD64C | dsc12:181–184 |
| +0x138 | SLICE_SIZE | 0x0014021C | 0x0014021C | 0xDD668 | dsc12:186–189 |
| +0x13C | MISC_SIZE / chunk | 0x0000021C | 0x0000021C | 0xDD674 | dsc12:191–192 |
| +0x140 | HRD_DELAYS | 0x020E0200 | 0x020E0200 | 0xDD698 | dsc12:194–197 |
| +0x144 | RC_SCALE | 0x00000020 | 0x00000020 | 0xDD6A8 | dsc12:199–200 |
| +0x148 | RC_SCALE_INC_DEC | 0x000701E8 | 0x000701E8 | 0xDD6C4 | dsc12:202–205 |
| +0x14C | RC_OFFSETS_1 | 0x0000000C | 0x0000000C | 0xDD6E8 | dsc12:207–210 |
| +0x150 | RC_OFFSETS_2 | 0x0516050E | 0x0516050E | 0xDD700 | dsc12:212–215 |
| +0x154 | RC_OFFSETS_3 | 0x10F01800 | 0x10F01800 | 0xDD71C | dsc12:217–220 |
| +0x158 | RC_OFFSETS_4 | 0x00000000 | 0x00000000 | 0xDD738 | dsc12:222–225 |
| +0x15C | FLATNESS_QP | 0x00002207 | 0x00002207 | 0xDD76C | dsc12:227–231 |
| +0x160 | RC_MODEL_SIZE | 0x00002000 | 0x00002000 | 0xDD774 | dsc12:233–234 |
| +0x164 | RC_CONFIG | 0x0331EF06 | 0x0331EF06 | 0xDD7B8 | dsc12:236–242 |
| +0x168 | RC_BUF_THRESH_0 | 0x382A1C0E | 0x382A1C0E | 0xDD7F0 | dsc12:292–306 |
| +0x16C | RC_BUF_THRESH_1 | 0x69625446 | 0x69625446 | 0xDD828 | dsc12:292–306 |
| +0x170 | RC_BUF_THRESH_2 | 0x7B797770 | 0x7B797770 | 0xDD860 | dsc12:292–306 |
| +0x174 | RC_BUF_THRESH_3 | 0x00007E7D | 0x00007E7D | 0xDD880 | dsc12:292–306 |
| +0x178 | RC_MIN_QP_0 | 0x00729480 | 0x00729480 | 0xDD8C8 | dsc12:309–325 |
| +0x17C | RC_MIN_QP_1 | 0x00739CE7 | 0x00739CE7 | 0xDD910 | dsc12:309–325 |
| +0x180 | RC_MIN_QP_2 | 0x0115A529 | 0x0115A529 | 0xDD958 | dsc12:309–325 |
| +0x184 | RC_MAX_QP_0 | 0x00B52504 | 0x00B52504 | 0xDD9A0 | dsc12:309–325 |
| +0x188 | RC_MAX_QP_1 | 0x00E6B16B | 0x00E6B16B | 0xDD9E8 | dsc12:309–325 |
| +0x18C | RC_MAX_QP_2 | 0x0138C60F | 0x0138C60F | 0xDDA30 | dsc12:309–325 |
| +0x190 | RC_RANGE_BPG_OFFSETS_0 | 0x3CF80002 | 0x3CF80002 | 0xDDA78 | dsc12:309–325 |
| +0x194 | RC_RANGE_BPG_OFFSETS_1 | 0x36E38E3A | 0x36E38E3A | 0xDDAC0 | dsc12:309–325 |
| +0x198 | RC_RANGE_BPG_OFFSETS_2 | 0x34D34D36 | 0x34D34D36 | 0xDDB0C | dsc12:309–325 |
| +0xF00 | DSC_CTL / PP input mux | 0x00000000 for PP0 | 0x00000000 | 0xDDB3C | dsc12:329–345 |
| +0xF04 | DSC_CFG | 0x00020001 | 0x00020001 | 0xDDBB0 | dsc12:248–269 |
| +0xF08 | DATA_IN_SWAP | no write in this vendor implementation | 0x0002C688 | 0xDDBBC; literal 0xDDBC8 | no enable-path counterpart |

In the table, `dsc12` means `techpack/display/msm/sde/sde_hw_dsc_1_2.c` at the vendor commit identified below.

The 31 vendor enable writes exclude Windows' additional DATA_IN_SWAP store. Vendor disable writes and the active enable list are separate: its disabled mux=0xF is not a missing enable operation. The original static audit left the vendor DF hardware-capability selector conditional; the later working-mainline harvest and authoritative project comparison close DF_CTRL as a cause for this phone. The Windows value follows the vendor latency formula: transmit delay 512, derived latency 1177 plus multi-slice latency 540, then `ceil(1717/540)=4` initial lines. Do not reinterpret the panel's RGB format as a hardware-capability selector.

## 5. Working mainline reference and live comparison

The 2026-11-05 postmarketOS netboot capture used mainline **7.2.9-sc7280**, with a correctly displayed fbcon/debug shell; it did not establish a completed general-purpose userspace boot. Resource allocation was one LM_0, PP_0, CTL_0 and DSC_0, output through INTF_1/DSI-1. Windows uses the same pipeline shape. The Fairphone 5 failure involving two mixers plus merge into one DSC block is an informative precedent but does not describe this topology.

| Working mainline measurement | Value |
|---|---|
| Mode | 1080×2400 @ 120 Hz |
| Horizontal timing | 1080 / 1106 / 1145 / 1181 |
| Vertical timing | 2400 / 2416 / 2437 / 2453 |
| Nominal uncompressed mode clock | 347639 kHz |
| DSI bit clock / PLL VCO | 687,035,888 Hz |
| Byte / byte-interface clock | 85,879,486 / 42,939,743 Hz |
| Actual INTF pixel clock | 114,505,982 Hz |
| Escape clock | 19,200,000 Hz |
| MDP core actual / requested | 379,999,951 / 333,804,240 Hz |

The nominal mode clock is not the actual compressed-domain INTF pixel clock. These are measurements from the working **120 Hz** mainline session, not measurements or required clock constants for Windows, and not proof that a future uncompressed **60 Hz** path can keep those clocks.

### Divergent values (harvest, with newer Windows error capture filled in)

Physical reference blocks: DSC_0 `0x0AE81000`, DSI0 `0x0AE94000`, INTF_1 `0x0AE36000`, CTL_0 `0x0AE16000`. DscProbe uses the display driver's existing virtual MMIO mapping; never use these physical addresses as driver RVAs. Windows configuration values come from multiple captures and the tuple experiment; the table is not one simultaneous cross-OS trace.

| Register / offset | Working mainline | Windows | Interpretation |
|---|---|---|---|
| DSC CMN_MAIN_CNF +0x000 | 0x00000000 read | 0 read; 0x100 written | Read-zero is also present with correct display |
| DSC DF_CTRL +0x100 | 0x095C0003 | 0x095C0004 | initial_lines 3 vs 4; derivation difference, closed |
| DSI CMD_MODE_MDP_CTRL +0x040 | 0x00000008 | 0x00100008 | Windows bit20 differs; no isolated causal evidence |
| DSI STREAM0_CTRL +0x058 | 0x021D0039 | 0x04390039 | Upper word count 541 vs 1081; Windows matches vendor packing |
| DSI STREAM0_TOTAL +0x05C | 0x096000B4 | 0x09600168 baseline | Widebus-consistent tuple experiment did not change noise |
| DSI CMD_MODE_MDP_CTRL2 +0x1B8 | 0x00110006 | 0x00110008 | Low bits differ; widebus bits20/16 set in both |
| DSI COMPRESSION_MODE_CTRL +0x2A8 | 0x39003941 | 0x00003901 | Vendor Android uses 0x3901 too; different working packing form |
| DSI HS timer location +0x0BC | 0x0000FFFF | 0x0004EA60 | Vendor constant 0x0003FD08; not a mode-clock derivation |
| INTF CONFIG2 +0x060 | 0x00001111 | 0x1111 or 0x0110 | Session variation; v3.8 reconfirms 0x1111 with noise |
| DSI FIFO_STATUS +0x00C | 0x11111210 | 0x99991290 | HS underflow group set only in Windows capture; OPEN |
| DSI ACK_ERR_STATUS +0x068 | 0x00000000 | 0x00000000 | Residual snapshot, not complete error history |
| DSI DLN0_PHY_ERR +0x0B4 | 0x00088888 | 0x00000000 | Working reference has nonzero status; bits alone do not certify failure |
| DSI TIMEOUT_STATUS +0x0C0 | 0x00000001 | 0x00000000 | Clearing/timing differences prevent historical inference |
| DSI ERR_INT_MASK0 +0x10C | 0x13FF3BE0 | 0x6FFFFBFF | Different masking configuration; 1 means masked |
| DSI INT_CTRL +0x110 | 0xAA20AA00 | 0x02000200 | Mixed interrupt enable and W1C status fields |
| DSI CLK +0x120 | 0x00002343 | 0x008037C1 | Raw clock/status comparison; not a direct Hz reading |

Matching live values further constrain routing: DSC control +0xF00=0, +0xF04=0x00020001, +0xF08=0x0002C688; DCS_CMD_CTRL=0x00013C2C; CTRL2=540; INTF mux=0x000F0000; CTL_TOP=0xF0020000; DSC_ACTIVE=1; INTF_ACTIVE=2. Different packetization across stacks does not justify replacing Windows constants that already match the vendor stack.

Mainline raw capture became possible in a later DEVMEM-enabled kernel using mmap; the earlier harvest subsection saying “unavailable” describes the first kernel only. This does not make unrestricted Android `/dev/mem` reads safe: direct reads there caused two silent resets, likely from access to unclocked blocks. That route is abandoned. DscProbe requires an already powered/clocked display and does not establish power safety from mapped addresses alone.

## 6. Newest observation: HS-lane FIFO underflow group

**PROVEN observation:** DscProbe **v3.8**, 2026-11-05, dsc2 with DSC **on**, captured FIFO_STATUS **0x99991290**. Working mainline captured **0x11111210**. Windows has bits19/23/27/31, the **0x88880000** high-speed-lane FIFO-underflow group; mainline has none of that group. There is also a low-bit difference, so describe the selected group explicitly rather than claiming the whole-word difference is only underflow.

The same Windows snapshot reconfirmed `pi+0x1A18=1`, fixed bit-clock override `cfg+0x1BC=0` (automatic clock calculation remains active), derived bytes field 1080, slice count 2, DF_CTRL=0x095C0004, CTRL2=540 and CONFIG2=0x1111. The internal field labeled bytes-in-slice carries a derived value 1080 here; do not confuse that software field name with the vendor formula's 540 compressed bytes per physical slice.

### Why the static error audit matters

- Windows **does** read/classify DSI errors: getter RVA `0x157BF0`, selective Write-1-to-Clear (W1C) helper `0x157D88`, and normal query `0x124570` selector 6.
- FIFO bits19/23/27/31 are absent from both classifiers and the clear helper's reverse mapping. Mask/unmask helpers also omit their corresponding mask controls (bits26/27/29/30); “unmasked by no helper” does not prove their current mask state.
- The diagnostic wrapper further removes HS-lane **overflow** details from its returned bitmap. Overflow and underflow are different groups.
- Normal query computes and clears selected errors, then preserves only Phase-Locked Loop (PLL) unlock in the upper summary. Non-PLL FIFO/timeout/ACK/PHY detail is discarded there; a later zero is not proof of a clean history.
- Command-transfer failure recovery exists, but resets the DSI controller and command FIFO. No FIFO/timeout-triggered full panel init-plus-PPS recovery chain was established by the audit.

The inspected address construction is `M=Q[image+0x227558]`, controller 0 at `M+0x94000`, controller 1 at `M+0x96000`. The raw error set is FIFO +0x00C, ACK +0x068, PHY +0x0B4, timeout +0x0C0, mask +0x10C, interrupt +0x110 and clock/status +0x120. v3.8 includes this set. FIFO state and sticky error bits coexist; indiscriminate writeback is unsafe, especially for INT_CTRL's mixed enable/W1C fields.

### Precedent and limits of the claim

The public [OnePlus 7T Pro transport-error record](https://github.com/Sr-0w/hotdog-linux-bringup/blob/main/docs/evidence/2026-08-17-dsi-dsc-transport-errors.md) reports the same all-lane `0x88880000` group accompanying DSC corruption; full panel reinitialization recovered that device, while a broader controller-reset experiment worsened behavior. This supports investigating the error class, not transplanting a reset fix. That case degraded after initially good display; spacewar is noisy immediately on enable.

**OPEN:** neither “underflow causes this noise” nor “this is normal Windows status” is established. One residual snapshot cannot distinguish a latched initiating event, a consequence of a different failure, or an innocuous transition artifact. It also cannot establish the relation to the CPU rendezvous freeze. The capture session later reset without successful GPU disable; offline inspection found ConfigFlags=0 and restored it to 1. No new dump survived that session or the subsequent write-out attempt, so an older dump cannot explain the new event.

### Discriminating experiments — not yet performed

| Experiment | Capture / control | What it can distinguish |
|---|---|---|
| Time-course across enable | Raw FIFO/error/mask snapshots at earliest powered point, first frame and later noise; record order and reset boundaries | Whether the group predates, coincides with or follows corrupted output |
| Compare DSC-off build | Same raw set under verified uncompressed host and panel configuration | Whether underflow is specific to the compressed path; a broken DSC-off setup is inconclusive |
| Clear-and-watch | Separate, narrowly reviewed W1C experiment, save initial value and observe reassertion | Sticky history versus continuing activity; capture before altering evidence |

DscProbe remains **read-only**. Clear-and-watch and test-pattern injection require separate experimental code; neither capability is provided by the probe. Instrumentation before the driver's own clear would preserve otherwise lost error detail, but is a proposal, not an applied patch. Unmasking new sources requires correct mappings, clear order and rate control; it is not a harmless observational change.

## 7. Enable ordering and panel documentation boundary

Static Windows sequence: construct calculator/PPS → DSI/panel bring-up callback → PPS send (`0x10FA6C`) → host encoder leaf/common write (`0xDD578`). The earlier CTL flush does not prove an early first frame: for the saved path it does not issue CTL_START. The first-frame kickoff and final hardware latch timing remain unresolved by that static chain.

The working [mainline Visionox driver](https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/refs/heads/master/drivers/gpu/drm/panel/panel-visionox-rm692e5.c) ends prepare with panel initialization through display-on, PPS (DT=0x0A), compression-mode enable (DT=0x07), then 28 ms wait. The captured downstream on-command table contains 106 records/955 bytes, no PPS or compression-mode packet; downstream PPS is generated separately. A source search finding no explicit downstream 0x07 sender is scoped to the inspected source and is not proof that no framework/host/firmware path ever emits it.

The v26/v27 PCFG insertion tests already tried 0x07 in two orders without fixing decode. A faithful-order experiment is still distinct: PPS → 0x07 must be the **final** commands, after display-on, with the wait and no later automatic PPS rearranging the result. The candidate driver hook is **RVA 0x10FA70** immediately after the normal PPS call. This is a planned experiment, not a claim that a missing 0x07 explains the fault.

**Documentation search result, 2026-11-05:** no public RM692E5 display driver IC (DDIC) datasheet/register reference was found after English/Chinese, PDF, document-repository and code searches (12 queries). Public module specifications cover electrical/mechanical/pin information without the required command meanings. Mainline Visionox and Fairphone/BOE initialization sequences are useful references, not annotated register manuals. The related public RM690B0 document supports FE page selection as a family pattern, but does not define RM692E5 D1/D2/B4 DSC controls. This is a dated negative search result, not proof that a document can never become public.

Consequently panel-side DSC-off values are empirical questions. `FE D1 / B4 01` and `FE D2` PPS-mirror writes cannot be converted to a verified disable recipe by analogy. In particular, **B4 01→00 is not an established patch**, and a different-resolution BOE sequence cannot supply this panel's DSC parameters.

## 8. DSC-off route — conditionally feasible, not implemented

Goal: obtain stable uncompressed **1080×2400 at 60 Hz** as a diagnostic/fallback. Static analysis proves partial non-DSC paths exist, but not end-to-end success. The master flag is **panel-info `pi+0x1A18`**; DSI's **`cfg+0x1A08`** refers to the same field because `cfg=pi+0x10`. Capability substructure +0x0C selects a hardware/calculation path; it is not a second master switch. The declarative XML/registry entry that initially sets the flag has not been identified.

Flag=0 skips encoder programming, removes INTF compressed input/widebus through the existing path, and suppresses generated PPS. Two holes prevent a flag-only experiment from being meaningful: the command builder skips the record-generating helper and leaves word count **0**; the packetizer's enable=0 branch skips compression register writes instead of explicitly clearing compression enable.

### Host prototype (proposal only)

Use the **pristine image with cumulative dsc1** (capw included), rather than dsc2, whose fixed CTRL2=540 would otherwise survive the off path. The following are instruction-level candidates from the feasibility audit, **not patch specifications in patches.json**; no byte encoding, signing or deployment has been produced for them here.

| RVA | Existing instruction | Proposed experiment | Purpose |
|---|---|---|---|
| 0x11180C | `ldr w8,[x20,#0x1a08]` | `str wzr,[x24]` | Force the already addressed master flag off |
| 0x111810 | `cbz w8,#0x111858` | `b #0x111858` | Skip DSC ratio/table path |
| 0x1269CC | `ldr w9,[sp,#0x58]` | `mov w9,#0xCA9` | Fixed 1080-wide RGB888 word count 3241 |
| 0x126B40 | `cbz w11,#0x126BA0` | `nop` | Reach explicit compression register programming with zero record |

This totals **four host instructions**, plus separate panel/mode/clock work. In the prototype the software word-count field can remain 0 while hardware WC becomes 3241; a sustained implementation should populate the builder's word count instead. The fixed-width prototype does not support arbitrary width, ROI or switching DSC back on. Skipping encoder setup alone also cannot guarantee a previously enabled encoder is off; verify teardown and active/enable registers.

| Expected observation for the proposed raw RGB888 path | Value / requirement |
|---|---|
| Master / object DSC enable | pi+0x1A18=0; object+0x36C=0 |
| STREAM0_CTRL / TOTAL | 0x0CA90039 / 0x09600438 |
| DSI compression | CTRL=0x3900 (bit0 clear); CTRL2/CTRL3=0 on pristine+dsc1 path |
| Widebus / compressed input | DSI MDP_CTRL2 bit20 clear; target INTF CONFIG2 bit0/bit12 clear |
| Host encoder / CTL | DSC_CFG bit0 clear, DSC_ACTIVE=0, correct flush and raw input binding |
| Clock / panel | Actual lane and INTF clocks accepted, PLL locked, panel truly uncompressed RGB888, matching 60 Hz timing |

The normal raw path accepts 16/18/24 bpp and rejects 30 bpp; 10-bpc DSC is not a reason to demand raw RGB101010. Raw RGB888 payload alone is `1080×2400×60×24 = 3,732,480,000 bit/s`, or **933,120,000 bit/s/lane for four lanes**, before packet overhead, transitions and the command transfer window. At equal refresh this is three times the 8-bpp compressed payload; relative to DSC at 120 Hz it is still 1.5 times the payload. Lower refresh does not automatically lower the required bandwidth.

The latest DSC-on capture shows clock override=0, so automatic clock calculation is available. It does not prove the future off build's requests, provider acceptance or actual clocks. An INTF target near **229 MHz** is only an estimate, not an established setting. Panel acceptance of uncompressed command mode, pixel format, disable commands and timing remain OPEN; a stopped PPS is not proof of decoder disable. Empirical variants may retain/remove FE groups or try standard compression disable, with readback and visible patterns deciding, rather than guessed vendor register meanings.

Success requires matching software state, live host registers and panel configuration **and** a correct stable full-frame image: solid colors, RGB order, horizontal/vertical lines, full-screen grid, grayscale and repeatability after cold boot; resume and further mode-set need later validation. Incorrect or black output from an incompletely configured off build cannot exclude a compression/framing defect.

## 9. Escalation ladder and remaining measurement limits

1. **DSC-off experiment:** implement the analyzed host candidate with empirical panel and clock work; first establish a valid raw baseline.
2. **Faithful final PPS→compression-mode sequence:** hook after normal PPS, send enable and wait 28 ms with no later traffic; compare against the working mainline ordering.
3. **Test-pattern injection bisection:** establish each generator's position relative to DSC and command-mode support, then compare INTF and DSI injection points with known signals. A downstream uncompressed pattern is not automatically valid compressed input to a panel expecting DSC.
4. **Display-only Windows driver, last resort:** use public implementations to build a software-rendered display path if bisection shows the required change cannot be reached in the binary driver. This is months-scale work without 3D acceleration, not an existing replacement.

Per-CPU self-recording instrumentation (design work known as GX74) is a separate reliability investigation intended to separate interrupt delivery, CPU acceptance and handler progress during rendezvous. It does not itself solve framing. Driver-side serialization cannot stop arbitrary system activity from forming the other rendezvous participant. Reducing flush bursts or choosing quieter enable timing remains mitigation work, not a proven root fix.

Known-input photographs can constrain stride/slice ordering/padding; panel graphics-memory readback, if implemented, would expose **decoded pixels**, not the hardware-generated compressed stream. Direct compressed-byte observation is unavailable in the current setup, and raw MMIO sweeps of unpowered blocks are not an alternative. No test-pattern writer, GRAM reader, DSC-off build or display-only driver is included in this repository.

## 10. Evidence provenance and reproducibility limits

This public record integrates the device findings in the project record; the GPU status/options snapshot; the 2026-11-05 mainline harvest; vendor reference GX-R7; write-list audit GX-R8; the DSI error-path audit; DSC-off feasibility; the subsystem start-order audit; and the dated panel-document search. Older “not yet captured” sections are superseded explicitly above, rather than silently mixed with newer evidence. ADSP findings are detailed in [subsystems](subsystems.md) and [known issues](known-issues.md).

Vendor formula references are kernel-source-relative names at commit `e652a4a6b85a11c92e35ec346255e0f3fd0be9a3`: `techpack/display/msm/sde/sde_hw_dsc_1_2.c` (configuration/thresholds/binding), `sde_encoder_dce.c` (topology and latency), `techpack/display/msm/sde_dsc_helper.c` (PPS/packet sizing), and `techpack/display/msm/dsi/dsi_ctrl_hw_cmn.c` (packetizer/error/reset operations). Source derivations and Windows RVAs are evidence locators; no private filesystem paths or binary inputs are distributed here.

Reproduction requires the hash-matched donor driver, fork firmware/ACPI identity and powered display state. The preserved patch applicator previously reproduced dsc2 byte-exact outside the signing region; that is patch reproducibility, not a successful display result. This documentation rewrite did not rerun hardware tests or independently regenerate every historic emulation result. A future report should preserve OS/driver/firmware identity, DSC mode, capture timing, raw registers, photo correlation, cleanup result and ConfigFlags readback, while removing device-owner identifiers before publication.
