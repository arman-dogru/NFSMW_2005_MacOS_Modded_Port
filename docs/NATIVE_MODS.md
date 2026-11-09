# Native USA gameplay port: engineering record

Work branch: `mods/faircops-xbox`. Pinned baseline: SpeedBreaker v0.2.0,
`312ce69796937fed118400cef6a1975c4cbe8659`.
No Windows plugin loading, PC addresses, dependency upgrades, or vehicle-limit
changes are part of the work recorded here. Speedbreaker mass is implemented and
smoke-tested; race police and native FairCops have positive Circuit and Sprint
validation, with further mode/regression testing in progress.

## Build compatibility (2026-10-29)

The original ARM64 link failure was reproduced both in SpeedBreaker and in a
six-line Foundation program using `NSString` class dispatch. Homebrew Clang
23.1.4 emits `objc_msgSendClass$...` references, while the selected linker is
`/usr/bin/ld`, Apple ld 1230.1. `clang++ -###` reports an assumed linker version
27037.1 to its compiler phase. This differs from the actual installed linker.

LLVM's [class-stub change](https://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20260309/821000.html)
enables these stubs when the assumed ld64 version is at least 1250. The
[Clang 23 options reference](https://releases.llvm.org/23.1.0/tools/clang/docs/ClangCommandLineReference.html)
documents `-fno-objc-msgsend-class-selector-stubs`. The minimal program links
and returns success with that flag alone. Disabling instance selector stubs is
unnecessary.

`runtime/CMakeLists.txt` checks support for this flag and applies it only to
SpeedBreaker's Objective-C++ sources. The four affected `.mm` compile commands
contain it; C/C++ and the translated guest code do not. Foundation, AppKit, and
CoreFoundation were already linked, so missing frameworks were not the cause.

Environment: macOS 26.6.2; macOS SDK 26.2; deployment target 15.0; ARM64;
Clang 23.1.4 for C/C++/Objective-C++; CMake 4.4.5; Ninja 1.13.2. Installed
dependencies are reused from `build/deps/share/speedbreaker-deps.json`.
The initial direct hardware query was denied, but the launched runtime reports
Mac15,12, Apple M3 (4 performance and 4 efficiency cores), 16 GB RAM.
GitHub issue/commit searches for the stub
failure returned no results, and the upstream default branch still pointed at
the pinned baseline when checked. This is not an exhaustive upstream audit.

Reproduce the build from this checkout, with existing dependencies and `ppc/`:

```sh
cmake -S . -B build/main -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
  -DCMAKE_PREFIX_PATH="$PWD/build/deps" \
  -DFFMPEG_XMA="$PWD/build/deps/ffmpeg-xma" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja -C build/main -j4 SpeedBreaker
file build/main/runtime/SpeedBreaker
```

The existing setup/dependency scripts were inspected, not rerun. No new
dependency was installed or downloaded for execution. The unpinned
`extract-xiso` checkout remains a pre-existing supply-chain limitation.

The full build passed and `file build/main/runtime/SpeedBreaker` reports
`Mach-O 64-bit executable arm64` (46 MB). Rebuilt Objective-C++ objects no
longer reference class-message stubs, while ordinary instance selector stubs
remain. `ctest --test-dir build/main -R '^(dev_profile_test|paths_test)$'
passed both tests after configuring with `-DSB_TESTS=ON`.

The first restricted launch could not access macOS displays (`SDL_Init:
The video driver did not add any displays`); it ran guest startup without
graphics and was stopped. A launch with desktop access initializes Vulkan
on Apple M3, displays the intro and menus, and uses the isolated profile.
This distinction matters: headless frame counters are not graphics validation.

Baseline visual/input smoke test passed in the packaged app: Quick Race >
Quick Play > Drag, Heritage & Rosewood, Fiat Punto, one AI opponent. The race
loaded, the opponent moved, throttle accelerated the player from 0 to 36 MPH,
and Start opened the pause menu. In-race logs reported approximately 60 fps
over this short test. The race was not completed; this is not a long-duration
stability or performance benchmark. Audio was muted for the test.

The existing packaging script also passed with desktop access:

```sh
python3 scripts/macos/make_app.py build/main build/dev-app --test
```

It creates the separate, ad-hoc-signed `build/dev-app/SpeedBreaker (test).app`
(62 MB, macOS 15.0+; seven libraries and MoltenVK 1.4.2 bundled), without
changing `/Applications/SpeedBreaker.app`. The first restricted packaging
attempt failed at `iconutil`; rerunning with desktop access succeeded.
An ordinary `--test` package still defaults to the installed app's profile.
For isolated Finder launches, also pass
`--dev-profile="$PWD/build/dev-profile"`. This embeds explicit data/cache/game
paths through Apple's [LSEnvironment](https://developer.apple.com/documentation/bundleresources/information-property-list/lsenvironment)
key and captures only the listed gameplay variables present during packaging.
Direct command-line launches still need their explicit environment or
`run_dev.sh`; Launch Services applies the embedded environment only to its
own launches. No saves or game assets are bundled.

## Isolated development runs

`NFSMW_USER_DIR` and `NFSMW_CACHE_DIR` explicitly override the data and cache
folders on all platforms, with relative values resolved against the working
directory. A data override also suppresses migration of old home profiles.
These variables should be set together for development. The installed app's
default behavior is unchanged when they are absent.

```sh
bash scripts/macos/run_dev.sh
```

This uses `build/dev-profile/data` for saves/settings/logs and
`build/dev-profile/cache` for caches; `game/files` supplies game data. It
does not copy installed saves. For the recorded baseline test only, a copy
of the backed-up profile was placed in the development data folder.

Before any experimental launch, the existing `save/` and `settings.toml`
were copied to `build/private-backups/pre-native-mods-2026-10-29`. All three
files were verified against SHA-256 hashes recorded in that ignored folder.
Do not commit or share that backup. The installed app and game assets are
left in place. `tests/dev_profile_test.cpp` checks path separation, ordinary
path fallback, and suppression of home-folder migration without writing files.

## USA guest mapping and native hooks

Only the verified USA XEX is in scope:
`aebdf3c14d1e4e4ed2c130d94bc1a0e97bd6d5bf39bdfdef33c3909c9cc4ba4c`.
The local hash was rechecked. The installed `game/files` symlink is preserved.
Local image/disassembly and scratch analysis remain in ignored
`build/private-analysis/`. Do not commit or share those files.

Native hooks replace the generated weak USA wrappers and call their original
`__imp__` bodies. Generated files and `config/nfsmw.toml` need no edits or
regeneration. PAL retains its own wrappers. All guest pointers are 32-bit;
fields and pointers are big-endian. Native pointers never enter guest fields.

| Function or structure | USA address/offset | Supporting evidence |
|---|---|---|
| AICopManager constructor | `0x820EA958`, root vtable `0x82065C9C` | Registers the `AICopManager` task/name; vtable observed live. |
| Manager task | `0x820F2320` | Receives root+8, subtracts 8, calls pursuit update at `0x820F237C`. |
| Pursuit update | `0x820F0270` | Traverses cops and pursuit collections; native balancing runs after original. Live calls observed in Circuit and Challenge pursuits. |
| Manager cop array | root +`0xC4`; data +`0xC8`, count +`0xD0` | Original traversal and live Challenge test with four cops. This array contains only cops. |
| Full IVehicle list | data `0x82C6F18C`, count `0x82C6F194` | Independently traversed by `0x820EC060`; live humans, AI racers and traffic observed. |
| IVehicle | vtable `0x8208AD18`, driver +`0x9C`, active +`0xB4`, AI +`0x54` | Constructor/getters and live observations; driver 0 human, 1 traffic, 2 police, 3 racer. |
| Vehicle lifetime | ctor `0x823D4750`, dtor `0x823D5BE0` | Constructor registers owner+`0xC0` IVehicle; destructor unregisters owner+`0x2C` Simable. Native IDs change when allocations are recycled. |
| Vehicle destroyed | `0x823D2C18` | IVehicle virtual getter calls its damage component. Adapter uses original getter. |
| Simable from IVehicle | vehicle -`0x94` | Getter bodies `0x823D2AD8`/`0x823D2AF0`, independently confirmed constructor offsets. |
| Cop AI | vtable `0x82065A78`; target +`0x68`, formal pursuit +`0x84`, goal hash +`0x8C` | Constructor `0x821079B8`, getter bodies `0x820E90D0`, `0x820E90F8`, `0x820E9128`. Cop AI vtable observed in Challenge pursuit. |
| Cop pursuit interface | AI +`0x800`, vtable `0x82065B58`, active byte +8 | Constructor, getter/setter `0x820E9350`/`0x820E9348`, clear path `0x8210ACB0`; Active interface accepted in live Circuit/Sprint assignments. |
| AITarget | vtable `0x82064E1C`, Simable +`0x2C`, valid byte +`0x3C` | Constructor `0x8210DFB0`; acquire/reset/update bodies. |
| AITarget::Acquire | `0x8210E118`, r3 target / r4 Simable | Validates the Simable registry (`0x829059FC`), stores target and updates position/direction. Adapter calls this function, not raw pointer writes. |
| Static roadblock goal | hash `0x9E55B2E3` | Xbox Jenkins hash routine `0x8231C2B8` applied to local `AIGoalStaticRoadBlock` string; excludes stationary holders. Live exclusion test pending. |
| CanSpawn | `0x820EB6A8`, manager root+`0x54` interface | Checks manager block +`0x60`, cooldown +`0x58`, global `0x82A2BD94`, then script/cinematic `0x82C5E750`. All four observed live. |
| Patrol population update | `0x820EE908` | Reads heat patrol budget, pursuit count and patrol-goal vehicles; calls `0x820EC940` when undersupplied. |
| Patrol vehicle / placement | `0x820EB768` / `0x8211BBF0` | Reuses or creates police vehicle, then finds a road spawn point. `0x820EB970` creates through the PVehicle factory. Live Circuit/Sprint creation and successful placement observed. |
| Active race status | pointer `0x82A2CB18`, state +`0x1A28`, parameters +`0x1A30` | Original race logic and live Circuit state 1 with valid parameters. Other modes require validation. |
| CopsInRace getter | `0x823380E0`; key `0x3918E889` | Local flag `0x800` or VLT byte fallback. |
| CopDensity getter | `0x8233A738`; key `0xDBC08D32` | Local +`0x2A` or VLT percent, mapped to enum 0..3. |
| ForceHeatLevel getter | `0x823385E0`; key `0xE4211F4F` | Stock generated getter; native heat override removed 2026-11-04. |
| Raw attribute lookup | `0x82148818` | Some loader reads bypass getters. Composed with existing language hook, returns private values for CopsInRace and CopDensity only; heat passes through. |
| Race Police Off | `0x823380E0` caller return `0x8233997C`, `0x8233A738` caller return `0x82339790`, `0x820EB6A8` | Zero the serialized race flag and density and deny the stock CanSpawn result for active competitive races. Other uses of the getters and spawn checks keep their normal results. |
| Quick Race Starting Heat | `0x821068E8` | The USA `SetWorldHeat` script resolves the human AI heat interface. Both vtables `0x82065610` and `0x820662D0` use the same setter (+8) and getter (+4, reads +`0x1C`). The latter was observed in a live Circuit. The native update sets heat once for each initialized local driver in Quick Race. |
| Auto Drive | `0x82367150` / `0x823671E8` | USA script registration `0x8235E8A8` names these ForceAIControl and ClearAIControl. They resolve the first or last local player via `0x82366FD8`, then use interface type `0x820E3D98`. The runtime queues the input to game update `0x823AFE20` and verifies the control flag through interface slot +`0x0C`. One local player only. |

Speedbreaker physics mapping, configuration and live mass readback are recorded
in [SPEEDBREAKER_MASS.md](SPEEDBREAKER_MASS.md). The contiguous 25/100/2 tuning
values at `0x828FB8D4` were considered but have no confirmed readers and are
**not patched**.

## Current gameplay configuration

Unlimited Speedbreaker is enabled by default on this modification branch and
saved in Settings > Game. Other additions are opt-in, and the default collision
mass multiplier is stock-equivalent. Set variables before launching
`bash scripts/macos/run_dev.sh`.

| Variable | Default | Meaning |
|---|---|---|
| `NFSMW_SB_MASS` | `2` | Activation-only collision mass multiplier, 0..1,000,000. Effective mass capped at 1e9 kg. |
| `NFSMW_SB_UNLIMITED` | saved, default `1` | Full charge, no drain or minimum speed; live Settings > Game switch. See [mapping and tests](SPEEDBREAKER_UNLIMITED.md). |
| `NFSMW_SB_LOG` | `0` | Bounded activation/deactivation and base-mass readback. |
| `NFSMW_RACE_POLICE` | `0` | Race police eligibility, attributes and CanSpawn overrides; positive Circuit/Sprint spawning. |
| `NFSMW_POLICE_DENSITY` | `3` | Minimum density enum 1..3 when race police enabled. |
| `NFSMW_POLICE_START_DELAY` | `3` | Cap the initial race spawn cooldown, 0..60 seconds. Loading and cinematic gates still apply. |
| `NFSMW_FAIRCOPS` | `0` | Stable ground-cop dispatch and independent player sightings. |
| `NFSMW_FAIRCOPS_SKIP_ROADBLOCKS` | `1` | Exclude static roadblock holders. |
| `NFSMW_FAIRCOPS_LOG` | `0` | Five-second dispatch timings and player pursuit requests. |
| `NFSMW_POLICE_PROBE` | `0` | Bounded read-only layout/spawn diagnostics around shared feature hooks. |

Heat uses the original game rules by default. The optional Quick Race starting
heat sets the local driver's heat once at event startup. The separate police
budget defaults to 25% extra per starting racer and never scales heat.

FairCops retains each valid assignment across roster changes and race exit.
It allocates free ground cars within 150 m using coverage, distance and
lifetime ID in that order. Stock single-car reinforcements can use an alternate
spawn anchor. A failed placement tries the stock anchor before activation.
The previous rebalance and rotation variables are retired.

| USA hook | Evidence and behavior |
|---|---|
| `0x820EE908`, return `0x820F2378` | Manager patrol update on the simulation task. Takes a racer snapshot and runs staggered stock `CanSeeTarget` checks via `0x82107F50` for AI-assigned cops. The probe uses an unregistered target and saves the per-cop hiding latch at pursuit interface +`0x64`. |
| `0x8210B2E8`, return `0x820EEEA4` | Patrol manager's request getter after its eligibility checks. One verified player contact can release an AI pursuit through `0x820E9770` and use the stock request to create the player pursuit. |
| `0x820E9348` | Stock pursuit-active setter clears the dispatch record on release, including pooled cop reuse. |
| `0x82117950` and `0x820E9418`, return `0x8211816C` | Pursuit task reads the minimum of its member sight timers into task +`0x168`. Independent sightings update the player timer. The member getter passes through stock samples for matching targets. |

The current race CanSpawn experiment bypasses only the global suppression when
race state is 1 with valid parameters, the manager block permits spawning,
cooldown has expired, and no script object is active. It does not clear the
global or bypass cinematic checks. Reset/enable wrappers `0x820F18F8` and
`0x820F21A8` cap the initial cooldown once after the originals; later task
ticks do not reset it. The configured cooldown still gates actual spawning. A positive CanSpawn result alone
does not prove model creation, road placement or pursuit engagement.

## EA Trax during races

The modded launcher enables `NFSMW_RACE_EA_TRAX_ONLY=1`. A direct
development launch uses stock selection unless the option is set.

| USA hook | Evidence and behavior |
|---|---|
| `0x823AFE20` | Game-thread frame entry publishes a race flag for audio. The race parameter getters check the loaded race type. `0x8232F7F0` finds the local racer by Simable and `0x82335150` sets its finish byte at +`0x21`. |
| `0x82201628`, `0x82201720`, `0x82201258` | Pathfinder's startup type, pursuit transition test and interactive start. The normal dynamic event is `0x016E7282`. |
| `0x82201818`, `0x82201B50` | Only the in-game scheduler calls after its streaming and skip checks use the race policy. Those calls use stock Pathfinder Stop `0x825C4B30` and StartLicensedMusic `0x82200ED8`. This covers Drag without changing the general Drag predicate. |
| `0x82203690` | Pathfinder event handler. The race override suppresses interactive score events and preserves the stock stop and start-screen events. |

## Recorded tests and remaining validation

- Baseline ARM64 build, package, menu, Quick Play Drag race, throttle and pause:
  passed as described above.
- `fair_cops_test` covers current local dispatch, coverage priority, the range boundary and invalid coordinates.
- `speedbreaker_mass_test`, `dev_profile_test`, `paths_test`, `language_test`:
  passed. Mass test also passed both sanitizers.
- Native Speedbreaker 1000x activation/deactivation: passed with 1547 kg base
  mass unchanged and override 1547000 -> 0; see its document for limits.
- Circuit, Ironwood Estates, Lexus, three AI: cops spawned after the original
  minute cooldown with the global suppression bypass. Native assignments were
  read back from AITarget, including all four racers over undersupply rotations;
  43 acquisitions and zero failed readbacks in the first run. Restart cleared
  cops and assignments, and reinitialized the spawn cooldown. Repeated 1000x
  Speedbreaker activation/deactivation still read 1547000 -> 0 with 1547 base
  mass unchanged. Vehicle placement can take additional time beyond cooldown.
- Sprint, Diamond & Union, Lexus, three AI, start heat 2: reset cooldown read
  60 -> 3 seconds. Vehicle creation, placement and pursuit engagement succeeded;
  the run reached five eligible cops, allocated 2/1/1/1 with all target readbacks
  matching. Native adapter averages about 2..6 microseconds in five-second
  windows, with an observed 138 microsecond peak; verbose probes were on, so this
  is a short diagnostic sample, not a benchmark. Race completed at 2:23.07 and
  displayed its winner/results screen, with a police car visible at the finish.
- Challenge pursuit #2: original game produced four cops; no competing racers.
  This confirms police availability and cop list mapping, not fair retargeting.

Traffic/cop impact comparison, Career and other race types, live roadblock
exclusion, heat changes, destruction, Save/Load and sustained performance
remain unverified. The car was driven during the Sprint without corresponding
CUA throttle commands, and the runtime reported a connected DualSense; input
regression/isolation needs checking before attributing that behavior. No capacity increase is justified by current evidence.
A PVehicle capacity check permits up to 64 simables in its forced-allocation
path; the tested four-racer race is far below that count.

The isolated test launched by direct environment is required: opening an
unconfigured test bundle by itself uses the original profile. During baseline
UI testing, querying a closed app accidentally reopened a second instance
without isolation; its window-height setting changed. The instance was stopped,
the setting restored from the verified backup, and all three original profile
file hashes matched again. Avoid querying a closed CUA app binding.

PC sources informed behavior only: [FairCops](https://github.com/arman-dogru/NFSMW_FairCops_Mod/blob/main/FairCops.cpp),
[Race Fixes](https://github.com/ExOptsTeam/NFSMWRaceFixes/blob/master/GRaceCustom.h),
[Bartender](https://github.com/rng-guy/NFSMWBartender/blob/main/Source/Headers/Features/Advanced/CopSpawnOverrides.hpp),
[Extra Options](https://github.com/ExOptsTeam/NFSMWExOpts/blob/master/NFSMWExtraOptions/ExtraOptionsStuff.h),
[SpeedFixer](https://github.com/rng-guy/NFSMWSpeedFixer/blob/master/Source/dllmain.cpp).
No PC address or object offset was transplanted. No Windows ASI is loaded.

## Shared playtest preparation (2026-11-01)

The native police integration is checkpointed locally as `079db26` and the
FairCops guest adapter as `54a9bc0`. Five focused tests passed again before
the shared test. Current installed saves/settings were copied and hash-checked
in ignored `build/private-backups/pre-user-validation-2026-11-01` (three files).
The test continues to use the existing development profile, not those originals.

The shared test package is prepared with race police/FairCops enabled, heat 2,
density 3, initial delay 3 seconds, stock rebalance/rotation intervals and
roadblock exclusions. Speedbreaker mass is 1000 for this test; the runtime's
unset default remains the stock 2. Target and mass logs are enabled and verbose
mapping probes are disabled. See [PLAYTEST.md](PLAYTEST.md) for the test order.
