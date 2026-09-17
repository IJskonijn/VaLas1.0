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
