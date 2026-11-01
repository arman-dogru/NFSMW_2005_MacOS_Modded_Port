# Shared native gameplay validation

Use `build/faircops-test/SpeedBreaker (test).app`. This package uses
`build/dev-profile/data` and `build/dev-profile/cache`. The installed app,
original saves and installed game files are left in place.

Prepared settings: police enabled, heat 2, maximum density, three-second initial
spawn delay; FairCops enabled with rebalance every 30 manager updates,
undersupply rotation every 600 updates and static roadblocks excluded;
Speedbreaker mass 1000. The normal runtime default mass remains 2.

The three-second setting makes spawning eligible; road placement and pursuit
engagement can take additional time. FairCops distributes active pursuit cops,
not inactive pool vehicles. Formal pursuit membership and AI bust/DNF rules
remain the game's own.

Start with Quick Race > Custom Race > Circuit, three opponents, three laps.
Ironwood Estates and the stock Lexus IS300 provide a repeatable reference.

1. Confirm ordinary steering/throttle/braking work and releasing controls has
   the expected effect. Report any unexplained autonomous input.
2. Drive for at least a minute. Note when police first appear and whether any
   cops follow or hit AI racers. Logs independently record each racer's assigned
   and actual cop-target counts.
3. Compare a traffic/cop impact with Speedbreaker off and on. At 1000 the effect
   should be conspicuous. Note controllability, launches, jitter or crashes.
4. After Speedbreaker ends, check normal driving and collisions again. Logs
   should show the override clear to zero with the base mass unchanged.
5. Restart and repeat one activation, then return to the menus. Check for old
   assignments, stuck physics or a transition crash.

After this pass: Sprint, Lap Knockout, Speedtrap, Tollbooth, Drag and Career
events; zero-opponent events; heat changes and roadblocks; destruction;
Save/Load; and a run with all modifications disabled. These are separate tests,
not implied by the earlier Circuit/Sprint successes.

Logs stay local in the development profile's `logs/` folder. The directly
launched shared run also writes `build/diagnostics/user-validation-2026-11-01.log`.
For feedback, report the race/course, car, approximate race time and what
happened. No game files or save uploads are needed.
