English | [日本語](subsystems.ja.md)


# Subsystem engineering notes

Device/software snapshot: **2026-11-05**, spacewar A063 / SM7325, Windows 11 ARM64 22H2 and the linked forks. See [status](../STATUS.md) for the concise inventory and [known issues](known-issues.md) for recovery. A functioning upstream provider does not prove all downstream consumers work.

## 1. Touch: FT3680 and pin-mux self-restore

The working driver is [spacewar-ft3680](https://github.com/frost-1256/spacewar-ft3680). Earlier touch failures after PnP re-scan were attributed to rail loss; direct measurement instead found GPIO52–55 remapped from mux 0 to mux 1, with output-enable cleared on GPIO53–55. The writer remains unidentified. The rail-collapse explanation for this rescan failure was withdrawn.

The integrated v3 self-restore checks those CTL fields **before each communication**, within the existing **BusMutex**. It restores only observed mux/OE deviations to their known good values. It does not disable/reload the device, reset the controller, vote power, or manipulate GPIO81/105, Resource State Coordinator (RSC) or System Power Management Interface (SPMI). Power-policy ownership remains FALSE. That narrow scope avoids racing platform power management merely to repair a pin assignment.

**PROVEN:** five re-scans triggered ten restores, all successful; a further five-minute observation saw no additional change, and touch recovery was visibly confirmed. The fix was integrated in the main touch repository. Later normal-speed Wi-Fi/touch coexistence is demonstrated. Long endurance, sleep/resume, other triggers and the identity of the remapping writer are not established by those tests.

### Input/reporting baseline

The working baseline omits Tip Pressure from the HID descriptor and uses the small ID/Tip/InRange/X/Y layout. An earlier descriptor with pressure correlated with repeated shell failures; removing it stabilized input. Reporting remains simple parse→report, with a parser-side ghost filter. Duplicate suppression previously broke long press/right-click; release debounce combined with dedupe disabled produced stuck touches. Any reporting change must verify tap, release **and long press**, not just pointer movement.

Bring-up included GPIO81 configured as input/pull-up, 100 ms polling and drain reads; interrupt-low observation was not established in the historical record. DSDT v21 lets the SPI connection open successfully, but the documented transfer baseline still uses `UseBb=1`; successful resource opening is not proof of migration to the hardware SPI transfer path.

Earlier raw boot-time RSC re-voting collided with the active platform power model and caused SOC_CRITICAL_DEVICE_REMOVED. Do not revive that experiment as the fix for rescan remapping. Older power-model questions and the now-working mux restoration are different issues.

## 2. Wi-Fi: two separate start gates

The SM7325 donor pack's expected firmware string is `00228`, while Nothing firmware reports **00816**. Edit the UTF-16LE `qcwlan7325.inf` string `WlanFWVer` to `00816`; `[fwVersion.reg]` writes it as `fwVersion`. This bypasses the version mismatch only; it does not repair SAR initialization.

Nothing's stock `bdwlan.elf` advertises Specific Absorption Rate (SAR) **v2**. The driver's `SarInit` v2 branch returned `0xC0000001`, aborting WLAN start. `sarv2-allow` changes that branch to return success. The existing [patch table](../patches/README.md) is the authority for its one-instruction byte replacement; a registry-only workaround did not solve this branch failure.

**PROVEN:** the version edit plus SAR patch permits normal-speed Wi-Fi with touch coexistence. This is a device-specific bring-up workaround, not certification of all SAR/power-policy behavior or other firmware combinations. Keep stock board firmware appropriate to this device; donor metadata is not a reason to replace its radio calibration arbitrarily.

Excessive `GlobalDebugLogOutput=1` logging was observed to flood kernel-debug transport and make operation extremely slow; use `0` outside deliberate debugging. Such logging-induced slowdown should not be confused with the startup gates. Recent baseline boots start the needed TrEE chain automatically; the old manual `tree_start` step is no longer required.

## 3. Bluetooth: ACPI Serial Bus revision

DSDT **v21** changes exactly the SPI and UART Serial Bus descriptor **RevisionID 2→1** for FTSX3680 and BTH0 respectively; the ten I2C descriptors are unchanged. Bluetooth UART Transport Driver then passed Code 10 and demonstrated startup, discovery, pairing, connection and headset audio on 2026-10-31.

The source of truth is the [Silicium-ACPI fork](https://github.com/frost-1256/Silicium-ACPI). A disassembly tool may print V2 macro names even when descriptor RevisionID is 1; blindly recompiling its textual output can undo the fix. Verify generated descriptor bytes and keep the UART/SPI resource changes separate from WLAN binary patches. The SPI opening success noted above does not change the touch transfer-path claim.

Bluetooth headset audio is working; internal speaker/microphone routing is still unavailable. The two results should never be merged into a claim that the phone's audio subsystem is complete.

## 4. ADSP: initialization ownership and firmware path

Audio DSP (ADSP) and Wireless Processor Subsystem (WPSS) share `qcsubsys7325.sys` with other subsystem devices, but have different creation paths and configuration. The **Qualcomm Subsystem Dependency Device (SSDD), QCOM0A20**, owns first-registration initialization of the global QMI mutex at **image+0x76568**. The **Qualcomm Service Manager Device (QSM), QCOM0A1E**, is a separate dispatch-table entry and does not perform that initialization.

The crash site calls `KeWaitForSingleObject` on the embedded object's address at RVA **0x4B214**. The matching initializer uses `KeInitializeMutex`; a valid object address can still contain uninitialized wait-list links. Repeated boot failures and successful post-sign-in runtime enable establish **start order** as the discriminator. Merely loading the common SYS, enabling QSM, or observing SSDD ConfigFlags=0 is insufficient to establish completed registration.

| Pack input / path | ADSP | WPSS |
|---|---|---|
| Base hardware ID | ACPI\QCOM0A1B | ACPI\QCOM0AE2 |
| Firmware configuration | Audio extension `ADSP\ImagePath=%13%\qcadsp7325.mbn` | Dynamically obtained main-image mode, WLAN publishes `%13%\WPSS.MBN` |
| Binary resources | RADS.bin | RWPS.bin |
| Protection domains | 2: `msm/adsp/audio_pd`, `msm/adsp/sensor_pd` | 0 in the reviewed baseline |
| Required interfaces | PIL_TZ, FASTRPC, GLINK | TFTP, PIL_TZ, GLINK |
| Volume-startup wait | WaitForVolStartupAppComplete=1 | WaitForVolStartupAppComplete=1 |

`%13%` means the installed **extension/package's own DriverStore directory**, not an arbitrary folder beside the subsystem SYS. Audio and PIL extension matching must actually apply to the board IDs. Hardware-key Resources/PDInfo values, software-key ImagePath/interface lists and service Parameters are distinct registry scopes; copying values into the wrong scope does not implement the pack's configuration.

The Windows pack uses MBN images: `qcadsp7325.mbn` exists in the pack, and the PIL loader has checked open-failure handling (loader status `0xE0148204`). Missing `.mdt`/`.bNN` segments are not the established explanation for this QMI crash. Firmware availability does not itself prove correct live installation, memory reservation, security acceptance or communication providers; the later successful runtime enable demonstrates the exercised startup path.

**PROVEN 2026-11-04:** SSDD already started → manual ADSP enable after sign-in → Status=OK, no problem code, PIL firmware start succeeds. Boot-time ADSP → same 0x0A call site. Keep ADSP disabled at boot and only test later after SSDD readiness. No full provider total order or general-purpose boot-order repair is implemented here. ADSP's protection-domain/service-registry work explains a plausible extra QMI path compared with WPSS, but the original complete upper crash stack is not available.

## 5. Battery: chain startup is not data delivery

The PMIC, PMIC GLink (`QCOM0A8E`) and battery miniport (`QCOM0A2A`) all start normally, but **Win32_Battery returns zero instances**. Charging works independently of a Windows capacity/percentage report. The battery transport is associated with the ADSP-side `PMIC_RTR_ADSP_APPS` channel on lpass, so runtime ADSP startup was a useful experiment, not a guarantee of data.

The chain formed at boot **without ADSP**; its devices did not accept runtime stop/restart, so late ADSP success could not cleanly reconstruct the chain. Whether a correctly ordered ADSP present from early boot yields battery data remains **untested**. This is not proof of missing battery hardware, a working gauge, or a proven early-ADSP cure.

## 6. USB gadget, host and Power Delivery

**Working:** gadget mode exposes Windows/ESP to a PC and is the servicing/recovery route for protected driver replacement, BCD and offline SYSTEM-hive changes. Shut down phone Windows first; exposing storage is different from sharing a live mounted filesystem safely.

**Not working:** USB host and USB Power Delivery negotiation. URS0 is not enumerated. The leading explanation is an ACPI `_DEP` dependency on the USB Type-C Connector System Software Interface (UCSI) device whose driver installation failed. That dependency explanation is **unproven**; the record does not establish that installing UCSI alone makes host mode or PD work. Charging success and gadget storage do not validate those paths.

## 7. Other functions and boundaries

UFS, buttons and charging are demonstrated. Cellular modem, Global Navigation Satellite System (GNSS), NFC, cameras, sensors, fingerprint and internal audio remain unavailable, with no established individual root cause in these sources. ADSP startup is infrastructure progress rather than evidence that every consumer is implemented. Future driver reimplementation or later Windows versions are ideas, not workarounds for these missing functions.
