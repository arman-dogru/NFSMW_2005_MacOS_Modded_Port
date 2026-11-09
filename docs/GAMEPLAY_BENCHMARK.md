# Gameplay performance comparison

The completed user race on 2026-11-01 and the unlimited-Speedbreaker validation
are functional tests. Both include interactive setup and Speedbreaker use.
Neither has a matched baseline, so their stutters cannot yet be assigned to
the native gameplay modifications.

Use the same course, car, opponents, laps, traffic, difficulty, catch-up,
resolution, graphics and frame-rate settings. Keep power/display conditions
constant. Warm both caches, let thermals settle, and repeat in reverse order
to check order/thermal effects. Do not compile, package, capture screenshots,
change focus or open Settings during a timed window. Exclude load/countdown,
pause, finish and menu time. Avoid Speedbreaker during the first comparison,
since slow motion also changes the scene's workload.

1. Compare the current executable with all gameplay additions off versus on.
   Off: `NFSMW_SB_MASS=2 NFSMW_SB_UNLIMITED=0 NFSMW_RACE_POLICE=0
   NFSMW_FAIRCOPS=0 NFSMW_SHOW_ALL_POLICE=0 NFSMW_RACER_POLICE_SPAWNS=0
   NFSMW_KEEP_POLICE_NEAR_RACERS=0 NFSMW_POLICE_POPULATION_SCALE=1 NFSMW_BOSS_REWARDS=0
   NFSMW_RACE_EA_TRAX_ONLY=0`.
   Keep verbose probes/logging off in both. This isolates
   gameplay options while using the same runtime/compiler/graphics build.
2. Check the earlier baseline executable separately. Its packaged build is
   `build/dev-app/SpeedBreaker (test).app`, metadata `312ce69-dirty`, created
   before the gameplay additions with the build/profile compatibility fixes.
   Its mass setter and charge updater retain generated weak wrappers, and it
   has no native pursuit-balancing adapter. Do not open this old bundle
   directly: it has no embedded isolated profile. Launch its executable with
   explicit isolated data/cache/game paths, or first prepare a separate,
   signed bundle with those paths. Keep the installed application untouched.
3. If a repeatable difference appears, test police enabled with FairCops off
   and on. Extra police also add vehicle/graphics/simulation work, separate
   from the cost of target balancing. Compare mass/unlimited separately with
   matched Speedbreaker use if needed.

The existing runtime logs each 120-frame reporting window's FPS, maximum
frame duration, command-processor time, thermal state and other counters.
`scripts/compare_gameplay_perf.py` compares explicit race-only intervals in
session-log seconds:

```sh
python3 scripts/compare_gameplay_perf.py <baseline.log> <modified.log> \
  --baseline-window <start> <end> --modified-window <start> <end>
```

It excludes boundary-crossing windows and combines FPS using total frames
over total estimated time. `--json` emits a report with source hashes for
local records. These aggregates cannot produce 1% lows. The runtime also has
`NFSMW_VBLANK_LOG=<absolute-path>` for individual presentation observations
(on macOS, a companion `.actual.csv` uses Metal's on-screen timing if exposed
by the driver). Measure its overhead equally in both runs before treating
it as a frame-time benchmark. No new profiler/dependency is needed.

Record the exact binary, settings, isolated profile/cache provenance, course,
car, run order, time boundaries and thermals with each result. Logs, saves,
caches and executables stay in ignored local build folders.
