English | [日本語](known-issues.ja.md)


# Known issues, crash mechanisms and recovery

Applies to the **2026-11-07** spacewar / Windows 11 22H2 baseline. The platform **0x101** freeze and the ADSP **0x0A** start-order crash are distinct defects. A display showing noise, a driver failing start and a system becoming unresponsive are also distinct observations; record which happened.

## 1. BugCheck 0x101: mutual processor rendezvous

**PROVEN mechanism:** captured stacks show two processors entering an inter-processor rendezvous associated with flush broadcasts and neither completing. The counterpart to graphics activity can be arbitrary system work; serializing only `qcdxkm7325` threads cannot prevent it. Processor pairs vary across runs, so disabling a fixed pair of CPUs was rejected as a targeted solution.

**OPEN root defect:** the evidence does not yet separate interrupt delivery, CPU acceptance and handler progress at the stopped processors. Ordinary crash dumps may not preserve their useful register context. Per-CPU self-recording instrumentation (GX74 design work) is intended to distinguish those layers; it is not a completed platform fix distributed here.

Observed exposure includes manual GPU enable, boot with GPU enabled, and rare heavy early-boot activity such as dump write-out even with the GPU disabled. Enabling display can itself induce other system work such as redraws, so waiting for quiet helps but cannot guarantee safety. The record favors leaving processor idle enabled; turning off idle is not an established workaround.

### Operational mitigations

1. Keep GPU disabled for boot, and avoid automatic enable/retry tasks.
2. Enable manually only for a specific short experiment, preferably in a quiet sign-in-screen window. If testing after sign-in, minimize background activity; neither condition eliminates the defect.
3. Gather the intended evidence promptly and disable the GPU; leaving the noisy pipeline running heats the phone.
4. Verify ConfigFlags=1 before every reboot following an enable. If cleanup did not run, repair offline before trying Windows again.
5. Limit unnecessary flush bursts and repeated enable cycles in experimental designs, without describing those changes as a proven fix.

These measures protect the working touch/Wi-Fi/Bluetooth baseline and reduce exposure. They do not establish that a future freeze is impossible, and do not show that DSI FIFO underflow causes the CPU deadlock.

## 2. ConfigFlags trap: runtime enable persists into the next boot

`pnputil /enable-device` can clear the graphics device's disabled flag persistently to **ConfigFlags=0**. The old assumption that runtime enable leaves boot configuration untouched was disproved by offline hive inspection. If the session crashes before disable, the next boot starts the GPU again. Repeated boot freezes can therefore be the previous enable's persistent aftermath, not a new patch regression.

The current helper `gpu_now` enables and polls status for five minutes; it does not disable, restore ConfigFlags or recover from a crash. A final “survived” message does not mean the next boot is safe.

### Verification ritual on a healthy running phone

Use an elevated prompt. Disable the device, then query the flag; do not infer persistence from the command's return code alone:

```bat
pnputil /disable-device "ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0"
reg query "HKLM\SYSTEM\CurrentControlSet\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags
```

Expected baseline is **REG_DWORD 0x1**, with disabled device state. If it is not 1, do not reboot into an assumed-safe configuration. Use the offline recovery path when necessary rather than retrying an unstable live session.

### Offline recovery example

Phone Windows must be stopped; use gadget mode to expose its volume. Example `W:` refers only to the verified phone Windows volume. Run these commands from the servicing environment's elevated prompt and keep the backup private/outside this repository:

```bat
copy W:\Windows\System32\config\SYSTEM W:\Windows\System32\config\SYSTEM.pre-recovery
reg load HKLM\PhoneSystem W:\Windows\System32\config\SYSTEM
reg query HKLM\PhoneSystem\Select
```

Determine the control set selected for the next normal boot using `Select\Default` and compare `Current`/`LastKnownGood`. The demonstrated install uses **ControlSet001**; the commands below assume you have confirmed that choice. If yours differs, substitute the actual control set. Query the key before editing:

```bat
reg query "HKLM\PhoneSystem\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags
reg add "HKLM\PhoneSystem\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags /t REG_DWORD /d 1 /f
reg query "HKLM\PhoneSystem\ControlSet001\Enum\ACPI\VEN_QCOM&DEV_0A36&SUBSYS_IDP07325&REV_0118\0" /v ConfigFlags
reg unload HKLM\PhoneSystem
```

Verify each operation succeeds, then reload the same hive, query the same value again and unload it before safely ejecting the volume. The second read verifies persisted state after unloading; a registry editor's display before flush is insufficient. Do not overwrite an earlier recovery backup or copy an entire SYSTEM hive from another phone/configuration. The normal baseline here is the exact value 1; preserve the original value in the backup for review if additional configuration bits exist in your installation.

If ADSP boot startup was also enabled, locate its actual enumerated instance and restore its disabled baseline in the same verified control set. Do not substitute the graphics instance path for ADSP or invent its instance suffix.

## 3. ADSP boot crash: BugCheck 0x0A

**PROVEN static ownership:** `qcsubsys7325.sys` waits at call RVA **0x4B214** (return **0x4B218**) on the embedded Qualcomm MSM Interface (QMI) mutex at **image+0x76568**. The caller passes the object's address, not NULL. That static storage remains zero-filled until initializer RVA **0x48210** runs. The initialization chain is SSDD registration callback **0xB3C50 → 0x4B6C8 → 0x48210**; registration invokes the initializer on first registration. SSDD is the **Qualcomm Subsystem Dependency Device / QCOM0A20**. QSM is the different **Qualcomm Service Manager Device / QCOM0A1E** and does not initialize this mutex.

Waiting on an uninitialized dispatcher object can dereference NULL internal wait-list links, even when the object address is valid. The static report could not recover the exact failing kernel field from the return address alone; the start-order conclusion is strengthened by the later paired runtime/boot tests. It should not be rewritten as a NULL mutex pointer, a firmware-file lookup failure or a requirement merely to enable QSM.

**PROVEN experiment, 2026-11-04:** after normal boot with SSDD already started, manually enabling ADSP after sign-in succeeds with Status=OK/no problem code and firmware startup through Peripheral Image Loader (PIL). Starting ADSP during boot instead reproduces **0x0A at the byte-identical call site**. Start **order**, not merely ConfigFlags=0 on SSDD, is the discriminator. The operating compromise is ADSP disabled for boot, optionally enabled later after SSDD is ready.

The pack contains `qcadsp7325.mbn`; the Audio extension installs it in its own DriverStore directory and supplies `ADSP\ImagePath`. A genuinely missing image has a checked loader-failure path, not this uninitialized QMI mutex. WPSS working for Wi-Fi does not prove every ADSP path is ready: ADSP declares audio/sensor protection domains and extra service-registry/QMI activity; the reviewed WPSS setup has zero protection domains. The precise higher caller on the original crash stack was not recovered, so that divergence is a supported explanation rather than a reconstructed full stack.

**PROVEN, 2026-11-07: the same race also hits WPSS, and an ACPI dependency fixes it.** On a fresh install with the RPEN/GLNK/SSDD chain present, boot intermittently bugchecked 0x0A (Arg1=0, Arg2=2) at the **same qcsubsys call site 0x4B214** waiting on image+0x76568; a dedicated-dump capture (section 4) showed the WDF work-item stack through the `INITIALIZE_QMI_SERVICES` state handler for WPSS (QCOM0AE2). The DSDT gave WPSS no `_DEP` on SSDD, so WPSS could enter QMI initialization before SSDD registered and initialized the mutex. Adding `Name (_DEP, Package (One) { \_SB.SSDD })` to `Device (WPSS)` removed the crash over repeated boots with Wi-Fi, touch and Bluetooth working. Without the RPEN driver (`qcrpen`, QCOM06E1) the crash does not appear, but GLNK (whose `_DEP` is IPCC and RPEN) never enumerates, SSDD stays absent and the Wi-Fi adapter never appears; removing RPEN from GLNK's `_DEP` let SSDD start but still produced no adapter (**EXCLUDED** as a substitute).

**OPEN:** whether the same `_DEP` on SSDD lets ADSP start safely during boot. Keep ADSP boot-disabled until tested.

Runtime ADSP start does **not** establish battery reporting, internal audio or sensors. The PMIC/battery chain formed at boot without ADSP, all its devices start, yet `Win32_Battery` remains empty; those chain devices rejected runtime stop/restart. Whether early, correctly ordered ADSP changes that result is untested. See [subsystem notes](subsystems.md).

## 4. Dumps and evidence preservation

A watchdog or hard freeze may return the phone to Android without a normal bugcheck dump. A subsequent boot intended to write a dump can also freeze. The v3.8 capture session and its later write-out attempt produced no fresh dump; an older ADSP 0x0A dump remained. Always inspect dump timestamps and correlate them to the actual session before attributing its crash code.

**Early-boot crashes and dedicated dump files (PROVEN, 2026-11-07).** A bugcheck early in boot, before the paging file is usable, leaves no dump with the default configuration, and the kernel debugger may not attach in time. Configuring a dedicated dump file in the offline SYSTEM hive captured such a crash:

```text
HKLM\SYSTEM\ControlSet001\Control\CrashControl
  DedicatedDumpFile    (REG_SZ)    = C:\DedicatedDump.sys
  DumpFileSize         (REG_DWORD) = 1024          ; MB
  CrashDumpEnabled     (REG_DWORD) = 2             ; kernel dump
  AlwaysKeepMemoryDump (REG_DWORD) = 1
```

After the crash, read the file offline from gadget mode; it begins with `PAGEDU64` and opens directly in the debugger. The file is owned by SYSTEM, so copy it with a SYSTEM-context task.

Preserve available probe output and logs before another capture overwrites them. DscProbe flushes stages to improve survivability, but an in-progress report can be partial and a completed file can predate the latest boot. Record whether the last action was enable, capture, disable or reboot, whether ConfigFlags cleanup occurred, and whether output showed black, noise or an image. Strip owner identifiers from any shared report; raw dumps and logs are not repository content.

## 5. Other recovery boundaries

- Keep KD off for ordinary boots; a configured debug transport can wait for an absent host. This is separate from 0x101.
- Never live-overwrite protected DriverStore files. Disable does not make direct live replacement safe; use offline/gadget servicing with matching re-signed files and verified packages.
- Do not add raw touch power votes during boot: an earlier experiment collided with active platform power management and caused SOC_CRITICAL_DEVICE_REMOVED. Use the established touch mux self-restore.
- Do not sweep Android display MMIO via `/dev/mem`: two silent resets ended that route. Mapped addresses do not guarantee powered/clocked hardware.
- Disabling/re-enabling the GPU is not proven to fully power-cycle the panel. Controller/FIFO reset, PHY preparation, panel init/PPS and physical reset/rails are separate scopes; skip flags and callbacks affect what actually runs.

The [install guide](install.md) gives the safe baseline; the [display record](display-investigation.md) explains why full decoder recovery and FIFO causality remain open.
