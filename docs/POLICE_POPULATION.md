# Racer-based police population and minimap

USA Xbox executable only, using the fingerprint in [NATIVE_MODS.md](NATIVE_MODS.md).
Native wrappers call the original generated functions. No PC patch offsets,
generated PPC source or game assets are included in this repository.

## Settings and behavior

Native Settings > Game saves these options and applies them live:

| Option | Saved key | Default | Launch override |
|---|---|---|---|
| Show All Police | `show_all_police` | On | `NFSMW_SHOW_ALL_POLICE=0/1` |
| Police Spawn Near Racers | `racer_police_spawns` | On | `NFSMW_RACER_POLICE_SPAWNS=0/1` |
| Keep Police Near Racers | `keep_police_near_racers` | On | `NFSMW_KEEP_POLICE_NEAR_RACERS=0/1` |
| Police Count at Four Racers | `police_population_scale` | 2 | `NFSMW_POLICE_POPULATION_SCALE=1..2` |

Spawning, retention and scaling require an initialized active race and
`NFSMW_RACE_POLICE=1` or `NFSMW_FAIRCOPS=1`. Minimap visibility also works outside
races. Environment overrides do not replace saved preferences.

The multiplier grows linearly from 1x with one racer to the selected maximum
with four, counting the human. The default factors are 1x, 1.333x, 1.667x, 2x;
integer budgets round down. A five-car budget becomes 5, 6, 8, 10. More than
four racers still receive at most 2x. Zero/unknown budgets and helicopter
counts are not increased. The original 64-simable allocation ceiling remains;
budgets are requests, not guaranteed populations.

**This doubles ground-cop budgets, not heat.** Heat values, progression and
Career/event heat limits follow the original game. The old development
`NFSMW_POLICE_START_HEAT` override was removed on 2026-11-04 and is ignored.

New single pursuit cars choose a live racer with the fewest actual active cop
targets, excluding static roadblock holders. The original road finder receives
that racer's position/velocity through a temporary unregistered AITarget during
placement. Original road selection, placement retries, clearance and models
remain. Successful placements seed FairCops, which validates the assignment
against current actors and fairness. No extra formal pursuit objects or AI bust
timers are created. Patrol/camera, roadblock and formation placement routines
remain original and may still originate near the human.

The manager's camera-distance cleanup test retains a cop inside the existing
radius of any live racer, including racers far from the human. Abandoned cops
outside every racer's radius can still be removed. Heat, destruction, pursuit
ending and other original cleanup conditions are not bypassed. Other streaming
or removal paths may exist; remote survival is not guaranteed in every situation.

Minimap cops no longer need to be pursuing the human. Original validity checks,
styles, transforms and clipping/fading remain. Eight stock ground markers are
supplemented with package-owned FEImage clones, up to 64 total. The minimap's
original tracking-node pool/list lifecycle is reused; the FE package owns clone
destruction. Eight extras are prepared over four HUD updates; further growth
occurs as needed, at most two clones per update. Disabling hides extra art and
restores the stock update. Helicopter art remains original.

## Independently mapped Xbox evidence

| USA address/layout | Evidence and use |
|---|---|
| `820F2320` | Manager task receives root+8; budget scope covers the original task. |
| Manager vtable `82065C9C`, ceiling `+98` | Constructor `820EA958` initializes 8; population update compares active `+94` against it. Scale temporarily, restore if unchanged afterward. |
| Attribute key `24F7A1BC` | Xbox hash of `NumPatrolCars`, read by `820EE944`. Private immutable return values avoid shared VLT edits. |
| `82118F80` | Caller-owned car-table rows: stride `18`, requested count `+10`, model hash `+8`. Original helicopter exclusion uses `82A2DF44`. |
| `820ECFB8` | Single-car placement, success in r3. Road finder `820ECA58` calls target getter `8211A428`, return address `820ECA94`; only this call receives the scratch anchor. |
| AITarget `+20/+30/+2C/+3C` | Position/velocity, Simable, valid byte; original Acquire `8210E118` validates/fills scratch data. Constructor registration is not invoked. |
| `8211B9E8`, return `820EEBD4` | Manager distance/visibility test; sphere vtable `820651D8`, radius `+8`. Only this result is extended near racers. |
| `82C6F18C/82C6F194` | Full IVehicle list/count. Vtable `8208AD18`, driver `+9C`, active `+B4`, destroyed getter `823D2C18`; Simable=root−94, position virtual slot `+60`. |
| `82C6F30C/82C6F314` | Dedicated cop list/count used by the original minimap. |
| `8229B840`, byte `82A2C5FA` | Minimap update/non-pursuit visibility gate. Byte temporarily set/restored; original loop has eight ground markers. |
| Minimap `+D0`, FEImage vtable `82080060` | First cop icon. Live readback confirms type 1, Clone `822FC150`: allocate 96 bytes, call base copy constructor `82300CE8`, copy image texture `+5C`. |
| FE object `+2C/+30/+58` | Data pointer/size/render cache. Copy constructor allocates/zeros art data; copy into the new allocation, keep cache empty. |
| `82302538`, `823024A8` | Package/group traversal starts `+4C/+64`; list headers eight bytes earlier. Verify count, tail and sibling links before insertion. |
| `82159FD0`, pool `82A2BF04` | Original 12-byte tracking-node allocation, also used by minimap constructor. Cleanup `82143B28` from `8229AD08` returns nodes. |
| `8229A030`, `8229B5F0` | Original vehicle-to-map vectors and marker position/clipping, reused for extra cops. |

Static mapping confidence is high from the verified local USA image/generated
code; runtime coverage is narrower. The [PC Helicopter Options art-copy behavior](https://github.com/eatincrispies/NFSMWHelicopterOptions/blob/master/HelicopterOptions/Helicopter/AIVehicleHelicopter.cpp)
was a reference; Xbox Clone, traversal, pool and lifecycle were independently
identified before use. No PC addresses/layouts were assumed compatible.

## Validation checkpoint, 2026-11-01

`ninja -C build/main -j4 SpeedBreaker` produced a native ARM64 executable.
`fair_cops_test`, `police_population_test`, `settings_test` and
`speedbreaker_mass_test` passed. Policy tests cover budget bounds/rounding and
spawn seeds under the original allocator, which has since been replaced by stable dispatch.

The initial Circuit smoke test (Ironwood Estates, Golf GTI, three AI racers)
recorded ground requests 3→6 at four racers, the spawn finder consuming an AI
anchor, and matching FairCops targets without failed acquisitions. Non-pursuit
patrol markers were visible. This does not yet establish maximum simultaneous
population, physical placement distances, retention coverage, or rendering
beyond eight markers. The expanded marker build reached the main menu; its
race/marker regression check is pending the next session.
No matched stutter benchmark has been completed.

`NFSMW_POLICE_SPAWN_LOG=1` enables bounded creation timings, budget/anchor and
retention readbacks, plus marker diagnostics. Disable diagnostics for performance
runs. Logs/disassembly stay in ignored local build folders.

Pending: more than eight active ground cops; widely separated racers;
chase end/heat transitions/destruction; restart/repeated package loads;
all settings off; Career/other modes; matched performance runs. More cars and
remote AI can add CPU/GPU load. Earlier completed user-race logs include large
graphics-submit stalls as well as other hitches; their cause is not established.
