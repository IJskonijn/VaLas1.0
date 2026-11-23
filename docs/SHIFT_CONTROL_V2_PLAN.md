# ShiftControlV2 Plan — non-blocking state machine + ATF-temp compensated pressure/timing

## Comparison (pressures & shift timing only)

| Aspect | Current VaLas `ShiftControl` (V1) | `7226ctrl` | `ShiftControlV2` |
|---|---|---|---|
| Shift decision | Manual, adjacent-gear only (button/lever edge-triggered) | Manual **and** automatic (gear/TPS/speed map) | Manual only (unchanged) — `decideGear()`/`gearMap` intentionally not ported, per [SHIFT_COMPARISON.md](SHIFT_COMPARISON.md) |
| Pressure source | Fixed per-transition `ShiftSetting` (MPC/SPC/TCC, 0-255 PWM), scaled by 3-level throttle % | 2D map (load % × oil temp °C) per transition, bilinear-interpolated, battery-voltage normalized | Existing `ShiftSetting` values + throttle scaling, plus a new **ATF-temp multiplier** (cold=more pressure, hot=less). No battery normalization — no VBATT sensor wired |
| Shift timing | Fixed per-profile `UpshiftDelay`/`DownshiftDelay` ms + throttle-based extra delay | `shiftTimeMap` (SPC% × oil temp) computed dynamically each tick | Same fixed per-profile delay + throttle delay, multiplied by a new **ATF-temp delay factor** |
| Execution model | Blocking: `vTaskDelay()` inside `upShift()`/`downShift()` runs the whole shift synchronously | Non-blocking: `preShift`/`shift`/`postShift` phases advanced every tick via `millis()`, `shiftBlocker` guards re-entry | Non-blocking phased state machine (`Idle → Applying → Reducing(3→2 only) → Finishing`), advanced every task tick via `millis()` |
| ATF temp data | Read live every 100ms by `sensorHandlerTask` into `initial_AtfTemp`, but not passed to `ShiftControl` | Central to nearly every map | `initial_AtfTemp` piped into `ShiftControlParameters` |
| V1/V2 selection | — | — | Runtime: when "Use Throttle Position" is enabled, `shiftControlHandlerTask` calls `ShiftControlV2` instead of `ShiftControl` |

7226ctrl's actual pressure numbers are not directly portable (different hydraulics/battery-% calibration — see [SHIFT_COMPARISON.md](SHIFT_COMPARISON.md)). What is transferable: temperature strongly affects shift duration, and a non-blocking phased execution model.

## Decisions
- V2 lives alongside V1 (new files), auto-selected at runtime by the existing "Use Throttle Position" config flag.
- Non-blocking phased state machine, simplified from 7226ctrl's `preShift/shift/postShift` (no boost pre-shift wait — no boost sensor here).
- ATF-temp compensation is a 3-point piecewise-linear curve (cold/warm/hot breakpoints for pressure% and delay%), applied on top of existing per-transition `ShiftSetting` + throttle scaling.
- No battery-voltage compensation (no VBATT sensor wired).
- ATF-temp compensation constants are exposed in the `ShiftConfig` web UI + JSON persistence, same pattern as `ThrottleSettings`.
- `gearlever->CompleteShiftRequest()` still fires immediately on request acceptance; `screenToDisplayValue` reset to `Main` is deferred until the phase machine reaches `Idle`.
- `shiftControlHandlerTask` tick drops from 100ms to 20ms so phase timing (including the 50ms 3→2 reduce step) resolves cleanly. No functional change for V1, which already blocks internally.

## Implementation phases
1. **Data plumbing** — `atfTempPtr` + `atfTempCompensationPtr` added to `TaskStructs::shiftControlParameters`; `atfTempCompensationPtr` added to `shiftConfigParameters`; wired in the `.ino`.
2. **`AtfTempCompensationSettings` struct** — added to `VaLas_Controller.h` (enable flag + cold/warm/hot breakpoints).
3. **`ShiftControlV2` class** — new `ShiftControlV2.h`/`.cpp`, phased state machine, ATF-temp + throttle scaling.
4. **`ShiftConfig` web/JSON support** — defaults, JSON (de)serialization, web form section for ATF-temp compensation.
5. **Runtime switch** — `.ino` creates+inits `shiftControlV2`, `shiftControlHandlerTask` picks V1 or V2 based on "Use Throttle Position", tick rate reduced to 20ms.
6. **Verification** — trace all transitions against V1 behaviour at neutral ATF temp, confirm V1 path unaffected when the flag is off.

## Further considerations
- ATF-temp curve breakpoints are estimated/adapted from 7226ctrl's `shiftTimeMap` trend, not measured on this hardware — a tunable starting point.
- The 5↔5+ overdrive lockup pressures (15/25/20 constants) are only throttle-scaled, not ATF-temp-scaled, to avoid touching TCC lockup feel; only the profile-driven line/shift pressures get ATF-temp scaling.




TO FIX:
ets Jul 29 2019 12:21:46

rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
load:0x3fff0018,len:4
load:0x3fff001c,len:1216
ho 0 tail 12 room 4
load:0x40078000,len:10944
load:0x40080400,len:6388
entry 0x400806b4
Begin program
WiFi AP started. Connect to SSID: VaLas_722.6_Controller
IP address: 192.168.4.1
Pass: 12345678
Guru Meditation Error: Core  1 panic'ed (Unhandled debug exception)
Debug exception reason: Stack canary watchpoint triggered (loopTask) 
Core 1 register dump:
PC      : 0x400875ec  PS      : 0x00060636  A0      : 0x3ffb0120  A1      : 0x3ffb0060  
A2      : 0x00000001  A3      : 0x3ffc8590  A4      : 0x3ffc8590  A5      : 0x00000001  
A6      : 0x00060620  A7      : 0x00000000  A8      : 0x80085c20  A9      : 0x3ffb0100  
A10     : 0x3ff000e0  A11     : 0x00000001  A12     : 0x3ffbeb08  A13     : 0x00000001  
A14     : 0x00060623  A15     : 0x00000000  SAR     : 0x0000001a  EXCCAUSE: 0x00000001  
EXCVADDR: 0x00000000  LBEG    : 0x4000c349  LEND    : 0x4000c36b  LCOUNT  : 0x00000000  

ELF file SHA256: 0000000000000000

Backtrace: 0x400875ec:0x3ffb0060 0x3ffb011d:0x3ffb0140 0x4014910f:0x3ffb0180 0x401491e6:0x3ffb01a0 0x400879c6:0x3ffb01c0 0x40087ec9:0x3ffb01e0 0x401518d7:0x3ffb0250 0x401214ce:0x3ffb0280 0x401216ea:0x3ffb02b0 0x401226a5:0x3ffb02e0 0x40124cc1:0x3ffb0310 0x40120420:0x3ffb0360 0x40120a95:0x3ffb0390 0x4011ff12:0x3ffb03b0 0x401467b7:0x3ffb03d0 0x4000bdbb:0x3ffb03f0 0x40001125:0x3ffb0410 0x400594e9:0x3ffb0430 0x400feea2:0x3ffb0450 0x400fef15:0x3ffb0480 0x400dce0b:0x3ffb04a0 0x400dcac3:0x3ffb04c0 0x400dcad5:0x3ffb04e0 0x4015aede:0x3ffb0500 0x400d5025:0x3ffb0530 0x400d5156:0x3ffb0560 0x400d53a0:0x3ffb0580 0x400d528d:0x3ffb05a0 0x400d54a4:0x3ffb05c0 0x400d6a41:0x3ffb0670 0x400d6c4d:0x3ffb1f40 0x400dad8a:0x3ffb1f70 0x400e4576:0x3ffb1fb0 0x4008a1ce:0x3ffb1fd0

Rebooting...
