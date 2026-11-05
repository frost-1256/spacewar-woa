English | [日本語](STATUS.ja.md)


# Device status — Windows 11 22H2 / spacewar

Snapshot: **2026-11-05**, Nothing Phone (1) A063 / SM7325, the Mu-Silicium and Silicium-ACPI forks linked in [README](README.md). “Works” records demonstrated functionality on the current device; it does not assert sleep/resume, endurance, certification or compatibility with arbitrary driver builds.

## Working baseline

| Function | Evidence / required mechanism | Boundary |
|---|---|---|
| Windows boot | Reaches logon; UEFI loaded with `fastboot boot Mu-spacewar.img` | UEFI is not flashed; standalone A/B dual boot is not implemented |
| Touch | FT3680 driver restores observed GPIO52–55 mux/OE changes before communication under BusMutex | Writer of the remap is unknown; long-duration and sleep/resume coverage is incomplete |
| Wi-Fi | Normal-speed connectivity with fwVersion `00816` INF gate and `sarv2-allow` binary patch | Uses Nothing firmware; patch is tied to the recorded original hash |
| Bluetooth | Start, discovery, pairing and headset audio demonstrated with DSDT v21 Serial Bus RevisionID 2→1 fix | Bluetooth audio success does not imply internal speaker/microphone support |
| USB gadget | Windows/ESP storage exposed to a PC for offline servicing | Separate from USB host and USB Power Delivery |
| Charging | Charging demonstrated | Windows battery capacity/percentage reporting remains absent |
| Physical buttons | Working in baseline | No claim that every application maps every key |
| UFS storage | Windows installed and booted from internal storage | Partitioning still changes device data |

The old touch-rescan “power rail collapse” explanation was superseded by measured pin remapping and the integrated self-restore fix. Wi-Fi and touch coexist; manual startup of the older TrEE dependency chain is no longer needed in the demonstrated baseline. See [subsystems](docs/subsystems.md).

## In progress

| Area | Established | Still open / operating policy |
|---|---|---|
| Display / GPU | `qcdxkm7325.sys` `dsc2` starts, mode-set runs and panel shows structured DSC noise | No correct driver-driven image; keep GPU disabled at boot (`ConfigFlags=1`) |
| DSC register configuration | PPS/encoder agreement, all 31 vendor active writes accounted for, packetizer constants match, live widebus tuple test fails to change noise | Configuration-value investigation is closed; framing/order and DSI error behavior remain open |
| DSI error domain | v3.8 capture: FIFO_STATUS `0x99991290`, working mainline `0x11111210`; Windows has HS-lane underflow group `0x88880000` | Neither causal nor normal status proven; time-course, DSC-off comparison and clear-and-watch proposed |
| DSC-off fallback | Static analysis finds an existing partial non-DSC path plus two concrete holes | Conditionally feasible, not implemented; host four-instruction prototype needs panel and clock experiments |
| ADSP | Runtime enable after sign-in succeeds (2026-11-04); boot enable reproduces 0x0A | Keep disabled at boot; SSDD registration must initialize shared QMI mutex first |
| Battery chain | PMIC, PMIC GLink and battery miniport all start | `Win32_Battery` returns no instances; early-boot ADSP configuration remains untested |
| Platform reliability | 0x101 stacks establish mutual inter-processor rendezvous | Delivery/acceptance/handler-level root defect remains under investigation; timing mitigations reduce exposure only |

The complete [display investigation](docs/display-investigation.md) includes the live register comparison, closed hypotheses, newest FIFO observation and the escalation ladder. The [known issues](docs/known-issues.md) explain the two different crash families and recovery.

## Not working

| Function | Known state / explanation |
|---|---|
| Battery gauge | Chain starts but Windows reports no battery; downstream data path unresolved |
| Internal audio / microphone | Unavailable; ADSP runtime start alone does not establish audio routing or codecs |
| Cellular modem | Unavailable; no established root cause in the supplied record |
| GNSS | Unavailable; no established root cause |
| NFC | Unavailable; no established root cause |
| Cameras | Unavailable; no established root cause |
| Sensors | Unavailable; ADSP sensor-domain configuration is not proof of sensor functionality |
| Fingerprint | Unavailable; no established root cause |
| USB host / PD negotiation | URS0 does not enumerate; leading hypothesis is `_DEP` on the UCSI device whose driver installation failed |

The USB dependency explanation is a hypothesis, rather than a confirmed firmware or hardware defect. Do not substitute guessed fixes for the missing evidence.

## Operational baseline

Keep kernel debugging off unless a debugger is deliberately attached. Keep GPU and ADSP disabled for boot. Enable GPU only for short, quiet, manual experiments; disable promptly to limit heat and exposure to 0x101. **Check ConfigFlags=1 after each enable and before reboot; repair offline after a crash if necessary.** Runtime enable changes persistent state, including when a helper script never reaches its final log message.

The same platform freeze has occasionally appeared during heavy early-boot work with GPU disabled. A GPU-disabled boot is a mitigation, not a guarantee, and a missing dump does not prove that no freeze occurred.
