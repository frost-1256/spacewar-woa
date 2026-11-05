# Device status — Nothing Phone (1) / spacewar, Windows 11 22H2

Last updated 2026-11-05.

## Works

- Boot to the Windows logon screen (UEFI is `fastboot boot`-ed, not flashed)
- Touch — FT3680 driver, including automatic restore of its GPIO pin mux if
  something remaps it
- Wi-Fi — connects at normal speed (requires the SAR patch and INF change in
  [patches/](patches/))
- Bluetooth — starts, discovers, pairs, and plays audio (DSDT Serial Bus
  revision fix)
- USB gadget mode (exposes the Windows/ESP volumes to a PC)
- Charging, physical buttons, UFS storage

## In progress / partial

- **Display / GPU** — the driver (`qcdxkm7325.sys`, variant `dsc2`) starts
  and the panel is driven, but the DSC stream decodes as garbage (structured
  noise). Register-level configuration has been audited end-to-end against
  the vendor and mainline Linux stacks and is closed; the remaining suspects
  are stream framing and the DSI error domain. The GPU is therefore kept
  **disabled at boot** (`ConfigFlags=1`) and enabled only manually for tests.
- **ADSP subsystem** — crashes if started during boot (a start-order defect:
  a QMI mutex that only the SSDD device initialises is used before that, in
  some orders). Enabling it manually after sign-in works. The battery chain
  (PMIC GLink, battery miniport) starts, but Windows still reports no
  battery.

## Not working

Battery gauge, speaker/microphone, cellular modem, GNSS, NFC, camera,
sensors, fingerprint, USB host / PD negotiation.

## Known platform defect

Intermittent **BugCheck 0x101 (CLOCK_WATCHDOG_TIMEOUT)**: two processors can
end up in a mutual inter-processor rendezvous that neither completes —
observed with the GPU enabled, at boot with the GPU enabled, and rarely
during heavy early-boot activity (e.g. crash-dump write-out) with the GPU
off. Mitigations in use: GPU off at boot, enable only in a quiet window at
the sign-in screen, disable promptly after each test, and verify
`ConfigFlags` returned to 1 before rebooting (an enable that is interrupted
by a crash leaves it at 0, which re-arms the GPU for the next boot).
