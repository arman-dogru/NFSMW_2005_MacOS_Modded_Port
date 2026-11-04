# All six Blacklist boss rewards

**Settings > Game > All Boss Rewards** is on by default, including existing
profiles without the new setting. After defeating a Blacklist rival, pick each
of the six cards. The two remaining-pick labels count down from six to zero;
the next accept continues normally. Each card can be granted once. The original
shuffle, cash, boss-car award call, marker inventory and reward animations remain.
With fewer than six cards available, the limit is the available count.

Saved key: `[game] all_boss_rewards = true`. Turn it off for the stock two-pick
limit. `NFSMW_BOSS_REWARDS=0/1` overrides the saved choice for one launch. The
modded run command uses the rebuilt `build/main/runtime/SpeedBreaker`, so it
includes this change automatically; no test flags or game-data edits are needed.
Rewards discarded after earlier completed boss selections are not restored.
Original garage/inventory capacity and award eligibility still apply.

## Xbox mapping and implementation

Only the verified USA executable is supported (fingerprint in [NATIVE_MODS.md](NATIVE_MODS.md)).
The [Extra Options PC behavior](https://github.com/ExOptsTeam/NFSMWExOpts/blob/8a094fe29d28195bdcf69122dbfe945f16315f9a/NFSMWExtraOptions/ExtraOptionsStuff.h)
changes four selection/count constants. Xbox locations and layouts below were
independently matched in the local USA image/generated code, not copied from PC
addresses. No generated code or proprietary data is published.

| USA function/address | Verified behavior |
|---|---|
| `82585F50` | FEMarkerSelection constructor; stores vtable `820A5C74`, visible count `+2C`, six candidate rows of 12 bytes at `+74`: type, parameter, selected byte. |
| `82586E28` | GetNumSelected sums nonzero marker types with selected flags. One native wrapper calls the original and adapts only the four callers below. |
| `82586868` | Return to notification handler `82586768`; signed comparison with 2 admits the next selection. |
| `82586998` | Return after the original reward is awarded; signed comparison with 2 enables continuation text. |
| `825870C0` | Redraw `82586E78`; original `2 - count` formats remaining picks, object hash `38DEAC6B`. |
| `82587158` | Second redraw `2 - count`, rival prompt object hash `D6C0E097`. |
| `82586768` | Original duplicate-card check and reward paths: type 18 calls boss-car award `822CB178`; type 19 adds cash to Career settings; other types enter the original 63-slot marker inventory at `82A391B0`. |

For those four callers, the returned signed count is shifted by
`available - 2`. Thus their unchanged comparisons and subtraction use the new
limit. For six cards, real counts 0..6 are viewed as -4..2. No selected flags,
saved progress, card types or parameters are changed by the wrapper. Other
callers still see the real count. Region, object vtable, available count and
selected count are checked before adapting. No PPC regeneration is required.

## Validation

The policy test checks all counts for one through six available cards, both
limit comparisons, both counter calculations, stock/off behavior and invalid
state. The settings test verifies the new saved option defaults to On.

The guest integration test runs the actual compiled native wrapper with the
original generated Xbox notification, count and redraw routines against a
synthetic in-memory screen. It checks stock two picks versus modified six,
both displayed counters, original cash/inventory writes, boss-car award-call
arguments, repeated-card rejection and continuation after the last pick.
The UI APIs and boss-car creation API are mocked; this is not a completed
in-game boss defeat, animation test or Save/Load verification.

```sh
ninja -C build/main -j4 SpeedBreaker boss_rewards_test settings_test
ctest --test-dir build/main \
  -R '^(boss_rewards_test|boss_rewards_guest_test|settings_test)$' \
  --output-on-failure
```

The guest test uses the configured compiler and extracts only the required
generated routines into ignored `build/main/tests/boss-rewards-guest/` on the
local machine. Its 4 GB guest address space is a sparse virtual mapping, not a
4 GB save or asset copy. It never opens a save or launches the game.
