# Native Speedbreaker collision mass (USA)

Set `NFSMW_SB_MASS` to the desired effective mass multiplier before launching
the development build. Its default is the stock Xbox value, `2`. For example:

```sh
NFSMW_SB_MASS=1000 NFSMW_SB_LOG=1 bash scripts/macos/run_dev.sh
```

Values from `0` to `1000000` are accepted, including decimals. Invalid, negative,
NaN, infinite and out-of-range values fall back to `2`. Zero clears the additional
collision mass during Speedbreaker. A value of `1` uses the car's ordinary mass.
The effective override is capped at 1,000,000,000 kg, with bounded intermediate
arithmetic. These limits prevent this hook from introducing floating-point
overflow; they do not establish that every permitted value is stable in the
game's collision solver. Start below the extremes.

This setting changes only the verified USA Speedbreaker activation call. It
does not change the vehicle's base mass, engine, suspension, recharge, minimum
activation speed or time scale. The separate [Unlimited Speedbreaker](SPEEDBREAKER_UNLIMITED.md)
setting controls charge and speed eligibility. The original deactivation clears the override.
PAL is unsupported and keeps its original wrapper.

## Xbox evidence

The verified USA XEX hashes to
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
Addresses below were traced in that image, not derived from PC offsets.

| Location | Evidence |
|---|---|
| `0x8246BF00` | LocalPlayer activation toggle, active byte +`0x80`; creates the activation event. |
| `0x82390A38` | Activation event applies the game's time scale and queries the human vehicle's physics interface. |
| `0x82390BA4` / return `0x82390BA8` | Calls the collision override setter with rigid-body mass multiplied by 2. This return address guards the native modification. |
| `0x82390BCC` / return `0x82390BD0` | Calls the same setter with zero on deactivation. Argument remains unchanged. |
| `0x82408380` | Interface slot 1 stores a float at this +`0x3C`. Native weak-wrapper override calls the original body. |
| interface vtable `0x8208CA18` | Derived body constructor `0x82408578` installs it at body root +`0x130`; IRigidBody is root +`0x48`. |
| `0x82409148`, reads near `0x82409454` | Collision preparation consumes root +`0x16C`, rescales the collision body's momentum, mass and inverse mass when the override is positive. |

No XEX, generated C++, image bytes, disassembly, or game assets are included
in this source change. No guest data file is edited.

## Recorded validation

On the Apple M3 development build, Quick Race Circuit, Ironwood Estates,
stock Lexus IS300, three AI opponents: activation at `1000` stored `1547000`
while base mass remained `1547`; deactivation stored `0` while base mass still
read `1547`. The Speedbreaker overlay appeared and the game continued through
a guardrail collision without a crash. This is a short activation/deactivation
smoke test, not a quantitative traffic/cop impact comparison or an extreme-value
stability claim.

`speedbreaker_mass_test` checks stock behavior, zero, ordinary mass, high values,
the cap, invalid settings and very large inputs. It also passes AddressSanitizer
and UndefinedBehaviorSanitizer. Traffic/cop impacts, repeated recharge cycles,
restart/transition cleanup and sustained high-value physics still need testing.
