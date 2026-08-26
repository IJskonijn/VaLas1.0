# VaLas 722.6 Manual Shift Comparison

## Scope

This comparison covers the current `VaLas1.0` controller and these two reference projects:

- `x:\Private Repos\VaLas1.0\VaLas_Controller`
- `x:\Private Repos\VaLas1.0-master\VaLas1.0-master\VaLas_1_00.ino`
- `x:\Private Repos\7226ctrl-master\7226ctrl-master\main`

The intended mode for this review is **manual shifting only**. Automatic shift decisions from the `7226ctrl` project should not be carried into the current controller.

## Executive findings

The current controller has the correct basic 722.6 shift-solenoid mapping, but its configurable shift values do not consistently describe the same transitions as the code that consumes them. This is the most likely software reason for shifts behaving differently from the original VaLas controller.

There are also hardware-output concerns that should be verified before applying power to the transmission:

1. The revised schematic uses a 5 V pull-up resistor on each N-channel MOSFET gate, so open-drain output is intentional. LOW pulls the gate down and turns the MOSFET off; releasing the pin with HIGH allows the pull-up to turn it on.
2. TCC was attached to an ESP32 LEDC PWM channel but was also controlled with `digitalWrite` during ordinary shifts. It now uses LEDC consistently, preserving the configured 0-255 duty value.

Y4 was previously attached to an ESP32 LEDC PWM channel while also being controlled with `digitalWrite`. That conflict has now been removed; Y4 is controlled exclusively as an open-drain digital output. This specifically affects the `3 -> 4` and `4 -> 3` paths. For this revised gate circuit, a HIGH command releases Y4 and the external 5 V pull-up activates the MOSFET.

The reference projects do not prove the correct electrical interface for this specific PCB or solenoid driver. Confirm the driver polarity, pull-ups, and safe inactive state against the schematic before vehicle testing.

## Solenoid mapping

All three projects agree on the physical shift-solenoid mapping:

| Transmission transition | Solenoid | Current `ShiftControl.cpp` | Original VaLas | `7226ctrl` |
|---|---|---|---|---|
| 1 -> 2 | Y3 | Yes | `select_twoup()` | `y3` |
| 2 -> 1 | Y3 | Yes | `select_one()` | `y3` |
| 2 -> 3 | Y5 | Yes | `select_threeup()` | `y4` in that project's naming |
| 3 -> 2 | Y5 | Yes | `select_two()` | `y4` in that project's naming |
| 3 -> 4 | Y4 | Yes | `select_fourup()` | `y5` in that project's naming |
| 4 -> 3 | Y4 | Yes | `select_three()` | `y5` in that project's naming |
| 4 -> 5 | Y3 | Yes | `select_five()` | `y3` |
| 5 -> 4 | Y3 | Yes | `select_four()` | `y3` |

The names in the `7226ctrl` source are board-specific. Its `y4` and `y5` labels do not correspond directly to the current project's `y4Pin` and `y5Pin`; the transition table is the reliable comparison.

## Shift sequence comparison

### Current controller

For a normal shift, `ShiftControl::upShift()` and `downShift()` do this:

1. Write the configured MPC and SPC PWM values.
2. Assert the selected shift solenoid HIGH.
3. Set TCC from the configured value.
4. Wait for the configured delay.
5. Write an after-shift MPC value, set SPC to zero, and release the shift solenoid.

The 3->2 path has an additional special sequence: after its delay, it halves MPC and SPC, releases the solenoid, waits 50 ms, and then applies the final MPC value. This resembles the original VaLas `select_two()` routine, but the current implementation omits the original 20 ms delay before asserting Y5.

The 5 <-> 5+ paths are handled separately. They wait, then change MPC/TCC and keep Y3 LOW. SPC remains zero. These paths do not energize a shift solenoid, so no Y3/Y4/Y5 solenoid click is expected. They are a torque-converter state change, not a normal gear change.

### Original VaLas manual controller

The original `VaLas_1_00.ino` is explicitly manual. Each transition has a dedicated routine with hard-coded values. The important patterns are:

- The selected shift solenoid is asserted only for the shift window and then released.
- SPC is normally zero after the shift.
- TCC is explicitly turned off for ordinary shifts.
- The 3->2 shift has a 20 ms pre-delay, a 600 ms shift period, a reduced-pressure phase, a 50 ms wait, and then a low MPC after-shift value.
- 5 -> 5+ and 5+ -> 5 use a 400 ms delay and do not assert Y3.
- 3->4 uses a longer 1200 ms shift delay.

### `7226ctrl`

The `7226ctrl` project supports both manual and automatic operation. Its manual path is separate from automatic gear selection:

- Manual requests select one adjacent gear at a time.
- `shiftBlocker` prevents another shift from starting while one is active.
- The selected solenoid is asserted HIGH during `doShift()` and released in `switchGearStop()`.
- SPC and MPC are written as PWM outputs and are set to zero at shift completion.
- TCC is explicitly set to zero at shift start.
- The project also performs sensor-based pre-shift and post-shift processing; those parts are not required for the current manual-only controller.

Its default manual pressure values are not directly comparable to VaLas values. `7226ctrl` stores each value as a percentage, then converts it to PWM using:

```cpp
PWM = (100 - configuredPercent) * 2.55
```

It also passes the same configured value to both SPC and MPC in the manual path. At 12 V, before battery compensation, the effective defaults are approximately:

| Transition | `7226ctrl` setting | Approx. MPC/SPC PWM | Original VaLas MPC/SPC | Current VaLas default |
|---|---:|---:|---:|---:|
| 1 -> 2 | 35% | 166 / 255 | 40 / 40 | 80 / 90 |
| 2 -> 3 | 72% | 71 / 255 | 80 / 80 | 80 / 80 |
| 3 -> 4 | 80% | 51 / 255 | 90 / 100 | 90 / 100 |
| 4 -> 5 | 80% | 51 / 255 | 100 / 120 | 100 / 120 |
| 5 -> 4 | 65% | 89 / 255 | 140 / 140 | 140 / 140 |
| 4 -> 3 | 65% | 89 / 255 | 140 / 140 | 140 / 140 |
| 3 -> 2 | 17% | 212 / 255 | 180 / 180 | 180 / 180 |
| 2 -> 1 | 35% | 166 / 255 | 40 / 40 | 0 / 0 |

The approximate PWM values are intentionally only a translation of the source code; `7226ctrl` applies battery-voltage normalization and its configured values may have been tuned for a different driver/transmission installation. The important conclusion is that `7226ctrl` is not using the same hydraulic calibration as VaLas. Its values are often substantially softer or stronger, and its 1->2/2->1 setting is particularly different from the original VaLas values.

### Timing difference

The original VaLas project uses fixed transition delays, mostly 600 ms, with 3->4 at 1200 ms, 3->2 including a 20 ms pre-delay and a 50 ms intermediate wait, and 5 <-> 5+ at 400 ms.

The current VaLas controller preserves that fixed-delay style through `ShiftSetting` profiles.

`7226ctrl` does not use an equivalent fixed delay per transition in the manual path. It starts the shift, then completes it through its periodic shift/sensor state machine. The shift can be held by pre-shift boost logic, and the actual stop is driven by the project's shift timing and N2/N3/sensor processing. Therefore, its pressure values and behavior should not be copied into the current fixed-delay manual controller without also adopting the associated state machine and validation logic.

Do not copy its `decideGear()` automatic logic, speed/throttle maps, or `fullAuto` path into the current project for this test phase.

## Configuration indexing and transition profiles

`ShiftConfig::CreateDefaultConfig()` labels settings as D1 through D5+, and its comments describe downshift values using the gear number in the label. However, `ShiftControl` indexes the settings using the gear **after** the shift:

```cpp
gear--;
downShift(..., *gear);
// downShift uses gearboxSettings[gear - 1]
```

That made the current defaults inconsistent with the transitions. The controller now selects the source profile explicitly:

- Normal upshift to target `gear`: `gearboxSettings[gear - 2]`.
- Normal downshift to target `gear`: `gearboxSettings[gear]`.
- `5 -> 5+`: profile index `4` (`D5`).
- `5+ -> 5`: profile index `5` (`D5+`).

For example, after a `3 -> 2` downshift, `gear` is 2 and `[gear]` selects index 2, which is D3, the profile for the shift that started in gear 3. Changing every lookup to `[gear]` would therefore fix downshifts but break normal upshifts. Based on the original VaLas routines, the pre-fix effective comparison was:

| Transition | Original intended values | Current settings selected | Assessment |
|---|---:|---:|---|
| 1 -> 2 | MPC 80, SPC 90, 600 ms | D2 up: 80, 80, 600 ms | SPC differs |
| 2 -> 3 | MPC 80, SPC 80, 600 ms | D3 up: 90, 100, 1200 ms | All differ |
| 3 -> 4 | MPC 90, SPC 100, 1200 ms | D4 up: 100, 120, 600 ms | All differ |
| 4 -> 5 | MPC 100, SPC 120, 600 ms | D5 up: 25, 0, 400 ms | All differ; TCC also differs |
| 5 -> 5+ | MPC 25, SPC 0, 400 ms | D5+ up: 0, 0, 600 ms | MPC and delay differ |
| 2 -> 1 | MPC 40, SPC 40, 700 ms | D1 down: 0, 0, 600 ms | Incorrect |
| 3 -> 2 | MPC 180, SPC 180, 600 ms | D2 down: 40, 40, 700 ms | Incorrect |
| 4 -> 3 | MPC 140, SPC 140, 600 ms | D3 down: 180, 180, 600 ms | Incorrect |
| 5 -> 4 | MPC 140, SPC 140, 600 ms | D4 down: 140, 140, 600 ms | Matches values |
| 5+ -> 5 | MPC 15, SPC 0, 400 ms | D5 down: 140, 140, 600 ms | Incorrect |

The table assumes the original VaLas values are the desired baseline. The current web editor can make this more confusing because it displays settings by D1-D5+ while the shift code consumes many values by destination gear.

## Output and timing risks to verify

### Integer percentage calculations

In `resetToGear2()` expressions such as:

```cpp
255 / 100 * 40
```

were evaluated with integer arithmetic as `(255 / 100) * 40`, producing `80`, not approximately `102`. The same issue affected the 33%, 30%, and 95% values. The controller now uses multiply-before-divide expressions such as `(255 * 40) / 100`.

### Open-drain outputs

The revised schematic uses `OUTPUT | OPEN_DRAIN` for the shift solenoids and PWM outputs. With the 5 V gate pull-up, writing LOW turns a solenoid off and writing HIGH releases the pin so the external pull-up turns the N-MOSFET on. This polarity must be retained in the software and verified at the MOSFET gate.

### PWM and digital writes on the same pin

The current setup previously attached Y4 and TCC to LEDC channels, then shift routines called `digitalWrite(y4Pin, ...)` and `digitalWrite(tccPin, ...)`. Y4 is now digital-only and TCC now uses LEDC consistently. The open-drain polarity is intentional for the revised schematic.

### Shift task timing

The gear-lever and shift-control tasks both run every 100 ms. A shift routine blocks its own task during `vTaskDelay`, which is acceptable for a manual one-shift-at-a-time design, but the shared gear and request values are not protected by a mutex. The request is cleared only after the shift routine completes. This should be tested with a held switch and with a second press during an active shift.

## Recommended manual-only test order

Perform these tests with the transmission safely unloaded or on a suitable test rig, and verify electrical outputs before connecting hydraulic pressure:

1. Disable CAN/pedal input unless it is the manual input being tested. Confirm there is no automatic gear decision task enabled.
2. Confirm the lever reports Drive or Reverse and that the initial software gear is 2.
3. With the vehicle disabled, observe that a single button press creates one request only. A held button must not repeat shifts.
4. Test each transition separately and record Y3/Y4/Y5, MPC, SPC, and TCC with a logic analyzer or oscilloscope.
5. Check that every selected solenoid returns LOW after the shift and that non-selected solenoids remain inactive.
6. Check 3->2 specifically for the intended pre-delay, pressure reduction, solenoid release, 50 ms wait, and final MPC.
7. Check 5->5+ and 5+->5 specifically as TCC/MPC state changes; do not expect a shift-solenoid click from these paths.
8. Repeat the sequence in Reverse only after Drive is correct. Reverse currently starts at software gear 2 and permits manual R2 -> R1 and R1 -> R2 transitions.

## Recommended implementation direction

For manual operation, retain the current edge-triggered request model and adjacent-gear limit checks. The current indexing now selects the source profile, but a future robust representation could name profiles by transition, such as `upshift_1_to_2`, `downshift_3_to_2`, and `select_5_to_5_tcc`, rather than relying on numeric index relationships.

Before changing hydraulic values, first resolve these software and electrical questions:

1. Confirm at the MOSFET gate that open-drain HIGH releases the pin and the 5 V pull-up means solenoid ON, while LOW means OFF.
2. Retain open-drain control for the revised gate circuit.
3. Use one output API per pin: GPIO for Y3/Y4/Y5, and LEDC PWM for MPC/SPC/TCC if PWM is required.
4. Keep the corrected integer percentage expressions.
5. Align each configurable profile with a specific transition and preserve the original manual timing as the initial baseline.

The current code does not appear to implement speed/throttle-based automatic shifting. Its manual-only behavior is therefore a suitable basis for testing, provided CAN/pedal inputs are configured intentionally and the output issues above are resolved or experimentally ruled out.

## Startup panic diagnosis

The controller's sensor task called `Sensors::OutputRpmToGauge()`, which wrote to `RPM_GAUGE_CHANNEL` (channel 4). That channel was not initialized or attached to `PIN_RPM_GAUGE_OUT` in `setup()`. The resulting `ledc_get_duty(...): LEDC is not initialized` message was followed by the core 1 abort/reboot loop. The optional RPM gauge forwarding is now disabled, so the uninitialized channel is no longer used.

The optional ELR high-idle PWM output was also never called by the application. Its pin and LEDC channel initialization has been removed for now. Engine RPM measurement remains active for internal data, but it is no longer sent to the optional external gauge.