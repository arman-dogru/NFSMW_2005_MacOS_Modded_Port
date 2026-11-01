# Unlimited Speedbreaker (USA)

This branch enables **Settings > Game > Unlimited Speedbreaker** by default.
It is a persistent gameplay option, available in ordinary builds as well as
test packages. Turning it off applies immediately and restores the game's
normal charge drain and speed checks; the full meter then drains normally.
The setting is saved as `[game] unlimited_speedbreaker = true` in
`settings.toml`. `NFSMW_SB_UNLIMITED=0` overrides it for one run without
changing the saved preference, for comparison with the original behavior.

With it on, charge is immediately full, never drains, and activation has no
minimum speed. There is no replenishment wait or driving-speed requirement.
The ordinary Speedbreaker button still switches it on and off. The original
cinematic and vehicle-state checks and reset/deactivation events remain;
cutscenes and invalid vehicle states can still switch it off. The collision
mass option remains independent, including its normal deactivation cleanup.
PAL has not been mapped and retains its original behavior.

## Xbox evidence and implementation

Addresses are from the locally verified USA executable, SHA-256
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
Native replacements call the generated original bodies. No PC offsets,
generated code edits, VLT tuning edits or executable-byte patches are used.

| Location | Evidence / native behavior |
|---|---|
| LocalPlayer constructor `0x8246B8D0` | Installs root vtable `0x820928F0` and Speedbreaker interface vtable `0x82092948` at root+`0x4C`; initializes charge float root+`0x84` to 1 and active byte root+`0x80` to 0. Both vtables are checked before a native charge write. |
| Toggle `0x8246BF00` | Changes active byte and posts the original activation/deactivation event. Left original. |
| Eligibility `0x8246BF78` | Checks blocking globals, Simable/IVehicle availability, speed, then three vehicle conditions. The complete original function still runs. |
| IVehicle slot 35 / `0x823D2C48` | Returns the float at IVehicle+`0x78` (speed in m/s). Only the call returning to `0x8246C004` inside eligibility receives at least 14 m/s, clearing its 30 MPH threshold. The verified conversion constant `0x820AFF5C` is 2.23699, minimum at `0x820634F4` is 30. All other callers and the stored real speed remain unchanged. |
| Input toggle `0x8246C0A0` | Receives root+`0x4C`, checks eligibility and positive charge before activation. Native wrapper fills charge first, so an empty meter works on the first button press; original manual toggle remains. |
| Reset `0x8246C150` | Writes charge and tail-calls the original off toggle. Native wrapper refills after that cleanup without activating. |
| Charge update `0x8246C188` | Saves f1/f2 as recharge/drain elapsed time, used only in charge arithmetic. Native wrapper fills charge and supplies zero elapsed charge times, then calls original and refills. Original invalid-state deactivation still runs. |
| Update speed read returning to `0x8246C200` | Still receives real speed. Stock recharge requires over 100 MPH (`0x82040200`), but a full, non-draining meter needs no recharge. |

Pointers remain 32-bit guest addresses and charge writes are big-endian
32-bit IEEE-754 `1.0f`. The wrappers validate alignment/range and exact
LocalPlayer vtables. Disabling the option leaves original arguments and
charge values untouched. Logging with `NFSMW_SB_LOG=1` adds bounded toggle
and five-second charge readbacks (at most 256 lines per process).

## Validation

The settings test covers immediate toggling, saved off/on values across
reload, and the temporary baseline override without overwriting the saved
choice. Live checks must also cover stationary activation, sustained use
past stock depletion, manual off/immediate on, disabled behavior and race
reset cleanup. Results are recorded after those checks, not inferred from
the setting or build success.
